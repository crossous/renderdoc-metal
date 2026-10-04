# SPDX-License-Identifier: MIT
import re
import sys
from pathlib import Path

for name in sys.argv[1:]:
    lines = Path(name).read_text().splitlines()
    expected = {}
    actual = {}
    for line in lines:
        match = re.fullmatch(r"(CHECK|Metal pass events:) EID=(\d+) events=([\d,]*)", line)
        if match:
            prefix, event, values = match.groups()
            events = tuple(int(v) for v in values.split(",") if v)
            if prefix == "CHECK":
                expected[int(event)] = events
            else:
                actual.setdefault(int(event), []).append(events)
    assert expected, f"{name}: no oracle checks"
    for event, events in expected.items():
        assert event in actual, f"{name}: EID {event} was never queried"
        assert all(v == events for v in actual[event]), f"{name}: EID {event}, expected {events}, got {actual[event]}"
        assert event not in events and all(v < event for v in events)
    print(f"PASS {name}: {len(expected)} pass lists match serialized ownership; selected EID excluded")
