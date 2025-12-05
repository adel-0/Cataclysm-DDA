#pragma once
#ifndef CATA_SRC_AGENT_MODE_H
#define CATA_SRC_AGENT_MODE_H

#include <string>

class input_context;
class JsonOut;

// agent_mode enables LLM agent control via JSON IPC on stdin/stdout.
// Similar to test_mode, this is set via command line --agent flag.
extern bool agent_mode;
extern int agent_timeout_ms;  // -1 for infinite wait

namespace agent
{

/**
 * Wait for agent input and return the action to execute.
 * Sends an observation JSON to stdout, then reads a command from stdin.
 * @param ctx The current input context (provides valid actions)
 * @return The action string to execute, or empty string on error/timeout
 */
std::string wait_for_input( input_context &ctx );

/**
 * Send an observation to the agent via stdout.
 * Includes screen content, game state, and valid actions.
 * @param jout The JSON output stream
 * @param ctx The current input context
 */
void send_observation( JsonOut &jout, input_context &ctx );

/**
 * Capture current screen content from curses buffers.
 * @param jout The JSON output stream to write screen data
 */
void write_screen( JsonOut &jout );

/**
 * Capture current game state (player, visible creatures, messages).
 * @param jout The JSON output stream to write game state
 */
void write_game_state( JsonOut &jout );

} // namespace agent

#endif // CATA_SRC_AGENT_MODE_H
