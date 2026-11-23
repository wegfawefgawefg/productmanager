# To do

## More agents

productmanager understands Claude Code and Codex. Each agent's part is small:
`src/sessions.cpp` + `src/transcript.cpp` for Claude Code, and
`src/codex_sessions.cpp` + `src/transcript_codex.cpp` for Codex (which chats
are running, which agents they have, and what a log line means). Everything after that
(tiles, layout, viewers, the strip) works on plain chats, agents and
activities. Each new agent gets its own reader that fills the same structures,
and tiles show which kind of agent a chat is.

Planned:

- **Codex**: done. Still to do: show what a code-mode `exec` script runs when
  it calls several tools at once (only the first is shown), and let a finished
  subagent's generated image stay visible on the leader tile.
- **Antigravity** (Google). Agent manager with several agents per workspace.
  Find where it keeps agent state and logs, and whether its browser agent's
  screenshots can show up like images do now.
- **Hermes**. Find its log format and session layout.
- **Gemini CLI**, **OpenCode**, **Aider** and others, as they come up.

For each one: where the logs live, how to tell a running chat from a closed
one, how subagents are recorded, and how to jump to the terminal it runs in.

## Phone

- Push a notification when a chat turns to "your turn".
- Check whether the Claude app opens `claude.ai/code/<session>` links itself,
  and how to do the same for Codex chats.

## Display

- Redraw only when something changed, so an idle strip costs nearly nothing.
- Show the last lines of a failed command, and `+12 -3` next to each edit.
- Lines from the leader to its agents, with a pulse when they message each
  other.
- Seven tiles currently leave one very wide tile on the last row; split rows
  more evenly (3-2-2).
- Remember where the strip was dragged across restarts.
- A key binding that opens a viewer on the current workspace.
