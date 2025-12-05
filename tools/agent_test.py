#!/usr/bin/env python3
"""
Simple test script for CDDA Agent Mode.

Usage:
    python agent_test.py [path_to_cataclysm_binary]

This script demonstrates how to interact with CDDA in agent mode.
It launches the game, reads observations, and sends actions.
"""

import json
import subprocess
import sys
import os
from typing import Optional


def run_agent_session(binary_path: str, max_turns: int = 10) -> None:
    """
    Run an agent session with CDDA.

    Args:
        binary_path: Path to the cataclysm binary
        max_turns: Maximum number of action/observation cycles
    """
    # Start CDDA in agent mode
    print(f"Starting CDDA in agent mode: {binary_path} --agent")

    proc = subprocess.Popen(
        [binary_path, "--agent"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1  # Line buffered
    )

    try:
        for turn in range(max_turns):
            print(f"\n=== Turn {turn + 1} ===")

            # Read observation from stdout
            line = proc.stdout.readline()
            if not line:
                print("No more output from game (EOF)")
                break

            try:
                obs = json.loads(line)
            except json.JSONDecodeError as e:
                print(f"Failed to parse JSON: {e}")
                print(f"Raw line: {line[:200]}...")
                continue

            if obs.get("type") == "error":
                print(f"Error from game: {obs.get('message')}")
                continue

            # Print observation summary
            print(f"Context: {obs.get('context')}")
            print(f"Valid actions ({len(obs.get('valid_actions', []))}): "
                  f"{obs.get('valid_actions', [])[:10]}...")

            screen = obs.get("screen", {})
            print(f"Screen: {screen.get('width')}x{screen.get('height')}")

            game_state = obs.get("game_state", {})
            player = game_state.get("player", {})
            if player:
                print(f"Player: {player.get('name')} at {player.get('position')}")
                print(f"Moves: {player.get('moves')}, Stamina: {player.get('stamina')}")

            messages = game_state.get("messages", [])
            if messages:
                print(f"Recent messages: {len(messages)}")
                for msg in messages[-3:]:
                    print(f"  [{msg.get('turn')}] {msg.get('text')}")

            creatures = game_state.get("visible_creatures", [])
            if creatures:
                print(f"Visible creatures: {len(creatures)}")
                for c in creatures[:3]:
                    print(f"  {c.get('name')} at relative {c.get('relative')}")

            # Print screen content (first few lines)
            lines = screen.get("lines", [])
            if lines:
                print("Screen preview:")
                for i, line_text in enumerate(lines[:5]):
                    print(f"  {line_text[:60]}")

            # Choose an action
            valid_actions = obs.get("valid_actions", [])
            if not valid_actions:
                print("No valid actions available")
                break

            # Simple demo: cycle through some common actions
            demo_actions = ["pause", "DOWN", "RIGHT", "UP", "LEFT", "look", "inventory"]
            action = None
            for a in demo_actions:
                if a in valid_actions:
                    action = a
                    break

            if not action:
                action = valid_actions[0]  # Default to first valid action

            print(f"Sending action: {action}")

            # Send action to game
            command = json.dumps({"action": action})
            proc.stdin.write(command + "\n")
            proc.stdin.flush()

    except KeyboardInterrupt:
        print("\nInterrupted by user")

    finally:
        # Clean up
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        print("\nSession ended")


def find_binary() -> Optional[str]:
    """Try to find the CDDA binary in common locations."""
    candidates = [
        "./cataclysm-tiles",
        "./cataclysm",
        "../cataclysm-tiles",
        "../cataclysm",
        "cataclysm-tiles.exe",
        "cataclysm.exe",
    ]

    for candidate in candidates:
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate

    return None


def main() -> int:
    if len(sys.argv) > 1:
        binary_path = sys.argv[1]
    else:
        binary_path = find_binary()
        if not binary_path:
            print("Usage: python agent_test.py <path_to_cataclysm>")
            print("Could not find cataclysm binary automatically.")
            return 1

    if not os.path.isfile(binary_path):
        print(f"Error: Binary not found: {binary_path}")
        return 1

    print(f"Using binary: {binary_path}")
    run_agent_session(binary_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
