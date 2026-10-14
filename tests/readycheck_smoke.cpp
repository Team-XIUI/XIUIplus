#include "modules/readycheck/ready_check_state.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
    auto Check(bool condition, const char* message) -> void
    {
        if (!condition) throw std::runtime_error{message};
    }

    auto Chat(std::string_view name, std::string_view body, uint8_t kind = 4) -> std::vector<uint8_t>
    {
        std::vector<uint8_t> packet(24 + body.size());
        packet[4] = kind;
        std::copy_n(name.begin(), std::min(size_t{15}, name.size()), packet.begin() + 8);
        std::copy(body.begin(), body.end(), packet.begin() + 23);
        return packet;
    }
}

auto main(void) -> int
{
    try
    {
        using namespace xiui;
        const std::array<ready_check_member, 3> roster{{{1, 0, 1, "Checker"}, {2, 1, 3, "Healer"}, {3, 6, 10, "Bard"}}};
        const auto packet = Chat("Healer", " \x1E\x01Yes\x1F\x01 \t");
        const auto message = DecodeReadyCheckChat(packet);
        Check(message && message->sender == "Healer" && message->body == "Yes", "chat decoder strips bounded color codes and whitespace");
        for (size_t size = 0; size < 24; ++size)
            Check(!DecodeReadyCheckChat(std::span<const uint8_t>{packet}.first(size)), "truncated packets are ignored");
        Check(!DecodeReadyCheckChat(Chat("Healer", "Yes", 3)) && !DecodeReadyCheckChat(Chat("Healer", "Ready Check!", 5)),
            "tells and linkshell chat cannot trigger or answer checks");
        auto special = Chat("Healer", "Ready Check!"); special[5] = 8;
        Check(!DecodeReadyCheckChat(special), "special DAT-format messages are ignored");
        auto unterminated = Chat("Abcdefghijklmno", "Yes"); unterminated.pop_back();
        Check(DecodeReadyCheckChat(unterminated)->sender == "Abcdefghijklmno" && DecodeReadyCheckChat(unterminated)->body == "Yes",
            "full sender and message fields need no null terminator");

        ready_check_state state;
        Check(!state.Start(std::span{roster}.first(1), 1, 1, 1000) && !state.Start(roster, 1, 42, 1000), "solo and nonmember checks are rejected");
        Check(state.Start(roster, 1, 1, 1000) && state.Mode() == ready_check_mode::tracker && state.Count(ready_response::ready) == 1 &&
            state.Remaining(1000) == 30 && state.Remaining(30001) == 1, "initiator starts ready with a 30-second monotonic deadline");
        state.Receive({"Outsider", "Yes"}, roster, 1, 1100);
        state.Receive({"Healer", "I said yes"}, roster, 1, 1200);
        Check(state.Count(ready_response::pending) == 2, "outsiders and conversational text never become replies");
        state.Receive(*message, roster, 1, 1300);
        state.Receive({"HEALER", "yes"}, roster, 1, 1400);
        Check(state.Count(ready_response::ready) == 2 && state.Count(ready_response::pending) == 1, "duplicate replies count a member once, case-insensitively");
        state.Receive({"Bard", "No"}, roster, 1, 1500);
        Check(state.Completed() && state.Count(ready_response::not_ready) == 1, "all replies complete the check early across alliance parties");
        Check(state.TakeSummary() == std::vector<std::string>{"Players not ready: Bard"} && state.TakeSummary().empty(), "summary is emitted only once");
        state.Receive({"Bard", "Yes"}, roster, 1, 1600);
        Check(state.Count(ready_response::not_ready) == 1, "late replies do not change completed results");

        state.Start(roster, 1, 1, 2000);
        state.Receive({"Checker", "Ready Check! Are you ready? Answer Yes, No, or /"}, roster, 1, 2200);
        state.Receive({"Healer", "Ready Check!"}, roster, 1, 2300);
        Check(state.Mode() == ready_check_mode::tracker && state.Deadline() == 32000, "own echoes and overlapping checks do not reset an active tracker");
        state.Receive({"Healer", "/"}, roster, 1, 2500);
        state.Tick(32000);
        Check(state.Completed() && state.Count(ready_response::no_response) == 1 && state.TakeSummary()[0] == "Players not ready: Bard",
            "timeout distinguishes unanswered members and includes them in the summary");

        state.Clear();
        Check(!state.Receive({"Outsider", "Ready Check!"}, roster, 1, 4000) && state.Mode() == ready_check_mode::idle,
            "nonmembers cannot open prompts");
        Check(state.Receive({"Healer", "Ready Check! Are you ready?"}, roster, 1, 5000) && state.Mode() == ready_check_mode::prompt &&
            state.Sender() == "Healer", "Lua-compatible trigger opens a prompt from a verified member");
        state.Receive({"Healer", "Ready Check!"}, roster, 1, 6000);
        Check(state.Deadline() == 35000 && state.Answer(7000) && !state.Answer(7001), "prompt echoes do not extend the deadline, and buttons can answer once");
        state.Receive({"Healer", "Ready Check!"}, roster, 1, 8000);
        Check(!state.Answer(38000) && state.Mode() == ready_check_mode::idle, "expired prompts never send an answer");
        state.Receive({"Healer", "Ready Check!"}, roster, 1, 8000);
        state.Receive({"Checker", "Yes"}, roster, 1, 8100);
        Check(state.Mode() == ready_check_mode::idle, "manual party-chat replies dismiss the local prompt without another response");

        state.Start(roster, 1, 1, 9000);
        state.Receive({"Healer", "Yes"}, roster, 1, 10000);
        state.Receive({"Bard", "\\"}, roster, 1, 11000);
        Check(state.Completed() && state.TakeSummary()[0] == "Players not ready: none", "all-ready checks send the original completion message");
        state.Clear();
        state.Tick(100000);
        Check(state.Mode() == ready_check_mode::idle && state.TakeSummary().empty(), "clearing a session cannot emit stale summaries");

        std::vector<ready_check_member> alliance;
        for (uint32_t i = 0; i < 18; ++i) alliance.push_back({i + 1, i, 1, "Longplayername" + std::to_string(i)});
        state.Start(alliance, 1, 1, 1000);
        state.Tick(31000);
        const auto summary = state.TakeSummary();
        Check(summary.size() > 1 && std::all_of(summary.begin(), summary.end(), [](const auto& line) { return line.size() <= 140; }),
            "18-member summaries split into safe party-chat lengths");
        for (size_t i = 1; i < alliance.size(); ++i)
            Check(std::any_of(summary.begin(), summary.end(), [&](const auto& line) { return line.find(alliance[i].name) != std::string::npos; }),
                "split summaries retain every unanswered member");
        std::puts("Ready Check protocol, roster, responses, timeout and summary checks passed.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Ready Check check failed: %s\n", error.what());
        return 1;
    }
}
