# Benchmark Tutorials

## Table of Contents

* [rc_pingpong](#rcpp)
* [ud_pingpong](#udpp)
* [rc_overflow](#rcof)
* [ud_overflow](#udof)

<a  name="rcpp"></a>

## rc_pingpong
* A simple example of how to use **RDMA read** to get server message from its memory.

```bash
$cd scripts/
$./bootstrap.py -f make.toml
$./bootstrap.py -f run.toml
```

<a  name="udpp"></a>

## ud_pingpong
* A simple example of how to use **two-sided RDMA** to send & recv ud message.
You can type message to send in console when running client and it will be received by server.

*P.S. Do not use scripts.*


<a  name="rcof"></a>

## rc_overflow
* An example to show what will happen by using **RC** transport mode when server's memory buffer is overflowed.

```bash
$cd scripts/
$./bootstrap.py -f examples/rc_overflow/make.toml
$./bootstrap.py -f examples/rc_overflow/run.toml

[server.cc:52] (RC) Pingping server listenes at localhost:8888
[server.cc:68] Register test_channel
[client.cc:49] ro_client ready to send message to the server!
[client.cc:70] client send 20 msg
[server.cc:95] server received 20 msg, no package loss
[rctrl.hh:91] stop with :3 processed.
```

<a  name="udof"></a>

## ud_overflow
* An example to show what will happen by using **UD** transport mode when server's memory buffer is overflowed.

```bash
$cd scripts/
$./bootstrap.py -f examples/ud_overflow/make.toml
$./bootstrap.py -f examples/ud_overflow/run.toml

[server.cc:76] server wake up
[client.cc:56] client ready to send pingpong message to the server!
[server.cc:90] package loss, last seq 4, current seq 6
[server.cc:90] package loss, last seq 9, current seq 11
[server.cc:90] package loss, last seq 14, current seq 16
[client.cc:75] client send 20 msg.
```