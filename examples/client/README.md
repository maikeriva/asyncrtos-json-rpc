# Get started with this example

The example need the following components:

- AsyncRTOS
- AsyncRTOS WiFi
- AsyncRTOS Websocket
- AsyncRTOS JSON-RPC

Add them first in the components folder, for example as git submodules:

```
git submodule add git@github.com:maikeriva/asyncrtos.git ./components/asyncrtos
git submodule add git@github.com:maikeriva/asyncrtos-wifi-client.git ./components/asyncrtos-wifi-client
git submodule add git@github.com:maikeriva/asyncrtos-websocket-client.git ./components/asyncrtos-websocket-client
git submodule add git@github.com:maikeriva/asyncrtos-json-rpc.git ./components/asyncrtos-json-rpc
```

Then simply build, flash, and monitor the project with IDF.

```
idf.py build flash monitor
```
