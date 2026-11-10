"""Find explicit XIUI+ messages and shared configuration metadata in C++ sources."""

from pathlib import Path
import re

TOKEN = re.compile(r'''//[^\n]*|/\*[\s\S]*?\*/|'(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*"|[A-Za-z_]\w*|::|->|[^\s]''')


def decode_literal(token):
    result = bytearray()
    escapes = {"a": 7, "b": 8, "f": 12, "n": 10, "r": 13, "t": 9, "v": 11}
    text = token[1:-1]
    at = 0
    while at < len(text):
        char = text[at]
        at += 1
        if char != "\\":
            result.extend(char.encode("utf-8"))
            continue
        char = text[at]
        at += 1
        if char in "xuU":
            digits = re.match(r"[0-9a-fA-F]+", text[at:])[0]
            if char != "x":
                digits = digits[:4 if char == "u" else 8]
            at += len(digits)
            value = int(digits, 16)
            result.extend(bytes([value]) if char == "x" else chr(value).encode("utf-8"))
        elif char in "01234567":
            digits = char + re.match(r"[0-7]{0,2}", text[at:])[0]
            at += len(digits) - 1
            result.append(int(digits, 8))
        else:
            result.extend(bytes([escapes[char]]) if char in escapes else char.encode("utf-8"))
    return result.decode("utf-8")


def source_messages(text, path=""):
    tokens = [(m[0], m.start()) for m in TOKEN.finditer(text) if not m[0].startswith(("//", "/*"))]
    words = [token for token, _ in tokens]
    messages = {}

    def collect(begin, end, metadata=False):
        at = begin
        while at < end:
            if not words[at].startswith('"'):
                at += 1
                continue
            position = tokens[at][1]
            message = ""
            while at < end and words[at].startswith('"'):
                message += decode_literal(words[at])
                at += 1
            identifier = metadata and (re.fullmatch(r"[a-z][a-z0-9_.:-]*", message) or message.startswith(("<", "/")))
            if message and (not metadata or re.search(r"[A-Za-z]{2}", message)) and not identifier:
                messages.setdefault(message, position)

    def arguments(begin, count, metadata=False):
        depth, argument = 0, 0
        for end in range(begin, len(words)):
            token = words[end]
            if depth == 0 and token in (",", ")"):
                collect(begin, end, metadata)
                argument += 1
                if argument == count or token == ")":
                    break
                begin = end + 1
            elif token in ("(", "[", "{"):
                depth += 1
            elif token in (")", "]", "}"):
                depth -= 1

    for at in range(len(words) - 3):
        namespace, separator, method, opening = words[at:at + 4]
        count = 1 if namespace == "i18n" and separator == "::" and method in ("Text", "Format") else \
            3 if namespace == "module_option" and separator == "::" and method == "Action" else \
            2 if namespace == "module_option" and separator == "::" and method in (
                "Toggle", "ToggleButtons", "Color", "Slider", "Choice", "Dropdown", "Element", "EditScope", "OverrideColor", "Text") else 0
        if count and opening == "(":
            arguments(at + 4, count)
        if separator == "(" and (namespace == "tooltip" or namespace == "add" and "ControllerConfigItems" in text or
                namespace == "info" and "general_module" in text or
                namespace in ("label", "field", "choice", "button", "entry", "heading", "toggle", "slider") and "modules/actionbars" in path):
            arguments(at + 2, 16, True)
        if separator in (".", "->") and method in ("description", "confirmation", "action_text", "submodules", "hint", "tooltip", "message_", "reason") and opening == "=":
            end = next((index for index in range(at + 4, len(words)) if words[index] == ";"), len(words))
            collect(at + 4, end, True)
        if "module_option" in text and namespace in ("option", "result") and separator == "." and method == "label" and opening == "=":
            end = next((index for index in range(at + 4, len(words)) if words[index] == ";"), len(words))
            collect(at + 4, end, True)
        if namespace in ("tooltip", "help", "legend", "readiness", "placeholder", "title") and separator == "=":
            end = next((index for index in range(at + 2, len(words)) if words[index] == ";"), len(words))
            collect(at + 2, end, True)
        if namespace == "return" and "modules/actionbars/action_" in path:
            end = next((index for index in range(at + 1, len(words)) if words[index] == ";"), len(words))
            collect(at + 1, end, True)
        if namespace in ("module_config_section", "native_menu_window"):
            end = next((index for index in range(at + 1, len(words)) if words[index] == ";"), len(words))
            for index in range(at + 1, end - 3):
                if words[index] == "{" and words[index + 1].startswith('"') and words[index + 2] == "," and words[index + 3].startswith('"'):
                    if namespace == "module_config_section": collect(index + 1, index + 4)
                    elif index + 5 < end and words[index + 4] == "," and words[index + 5].startswith('"'): collect(index + 5, index + 6)
    return messages


def referenced_messages(root):
    messages = {}
    sources = list((Path(root) / "src/runtime").rglob("*")) + [Path(root) / "src/shared/native_menu_windows.hpp"]
    for path in sorted(sources):
        if path.suffix not in (".cpp", ".hpp") or "resources" in path.parts:
            continue
        text = path.read_text(encoding="utf-8")
        for message, position in source_messages(text, path.as_posix()).items():
            location = f"{path.relative_to(root).as_posix()}:{text.count(chr(10), 0, position) + 1}"
            messages.setdefault(message, location)
    return messages
