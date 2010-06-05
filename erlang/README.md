MessagePack-RPC Erlang

the code is under construction. it works in test environment.

## design

## client

  gen_server - mp_client

1. connect to server with specifying address and port.
2-1. append mp_client after some supervisor if you want to
     keep connection.
2-2. close it after RPC call ends.

## server

mp_server -+- mp_server_sup -+- mp_server_srv
                             +- mp_server_sup2 -+- mp_session

1. set callbacks by passing Module into mp_server_sup:start_link/3.
2. set mp_server_sup under your application.
   (you can set multiple mp_server_sup)

see mp_server.erl for detailed usages.