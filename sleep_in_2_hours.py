#!/usr/bin/env python3
"""Suspend this Fedora/KDE computer in two hours. Ctrl+C cancels."""

import subprocess
import time


def main():
    print("PC will sleep in 2 hours. Keep this running; Ctrl+C cancels.", flush=True)
    try:
        time.sleep(2 * 60 * 60)
        subprocess.run(["systemctl", "suspend"], check=True)
    except KeyboardInterrupt:
        print("\nSleep cancelled.")


if __name__ == "__main__":
    main()
