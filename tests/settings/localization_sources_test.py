import importlib.util
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[2] / "scripts/localization_sources.py"
SPEC = importlib.util.spec_from_file_location("localization_sources", SOURCE)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class LocalizationSourcesTest(unittest.TestCase):
    def test_messages_and_metadata(self):
        text = r'''
        // i18n::Text("Ignored comment")
        auto label = i18n::Text(selected ? "Previous choice" : "Next choice");
        auto text = i18n::Format("Slot %zu", slot);
        return module_option::Toggle("Enabled", "Show this module.", &value);
        static module_config_section sections[]{
            {"Extra Settings", "Optional behavior.", "extra-settings"}};
        option.description = available ? "Available now." : "Try again later.";
        '''
        messages = MODULE.source_messages(text)
        self.assertEqual(set(messages), {"Previous choice", "Next choice", "Slot %zu", "Enabled",
            "Show this module.", "Extra Settings", "Optional behavior.", "Available now.", "Try again later."})

    def test_utf8_and_adjacent_literals(self):
        text = r'i18n::Text("Language / " "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E")'
        self.assertEqual(set(MODULE.source_messages(text)), {"Language / 日本語"})

    def test_dynamic_content_is_not_reclassified(self):
        text = 'context.DrawText(item.name.c_str(), bounds, style); result.label = row["displayName"];'
        self.assertEqual(MODULE.source_messages(text), {})


if __name__ == "__main__":
    unittest.main()
