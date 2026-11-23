# To do

## More agents

Today productmanager only understands Claude Code. The Claude-specific part is
small: `src/sessions.cpp` (which chats are running, which agents they have)
and `src/transcript.cpp` (what a log line means). Everything after that
(tiles, layout, viewers, the strip) works on plain chats, agents and
activities. Each new agent gets its own reader that fills the same structures,
and tiles show which kind of agent a chat is.

Planned:

- **Codex** (OpenAI's Codex CLI). Sessions are JSONL rollouts under
  `~/.codex/sessions/`. Map its shell, apply-patch and read events onto the
  same activities. Find out how to tell a live session from a finished one.
- **Antigravity** (Google). Agent manager with several agents per workspace.
  Find where it keeps agent state and logs, and whether its browser agent's
  screenshots can show up like images do now.
- **Hermes**. Find its log format and session layout.
- **Gemini CLI**, **OpenCode**, **Aider** and others, as they come up.

For each one: where the logs live, how to tell a running chat from a closed
one, how subagents are recorded, and how to jump to the terminal it runs in.

## Display

- Redraw only when something changed, so an idle strip costs nearly nothing.
- Show the last lines of a failed command, and `+12 -3` next to each edit.
- Lines from the leader to its agents, with a pulse when they message each
  other.
- Seven tiles currently leave one very wide tile on the last row; split rows
  more evenly (3-2-2).
- Remember where the strip was dragged across restarts.
- A key binding that opens a viewer on the current workspace.
