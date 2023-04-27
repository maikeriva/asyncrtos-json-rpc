# AsyncRTOS JSON-RPC modules

JSON-RPC server, client, and peer modules for ESP-IDF, built on AsyncRTOS.

## How do I use these?

Check out the examples folder.

## How do I contribute?

Feel free to contribute with code or a coffee :)

<a href="https://www.buymeacoffee.com/micriv" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>

## What needs work

### Client

- Support for batch messaging
- Investigate feasibility of replacing esp_timer API with FreeRTOS timers
- Provide task variant

### Server

- Keep track of IDs and check duplicates
- Do not free if some calls haven't completed processing
- Provide task variant

## Peer

- Provide task variant
