# Benchmark Performance

## Table of Contents

* [RW Basic](#rw_basic)
* [RW Flying request](#rw_fly)
* [RW Outstanding request](#rw_or)
* [RW Doorbell Batching](#rw_db)


<a  name="rw_basic"></a>

## RW Basic

```bash
$make bench_client
$./bench_client -client_name val08 -threads 12
```


   |          Setup          | per-thread | peek    |
   | ----------------------- | ---------- | ------- |
   | 1 clients * 12 threads  | 0.48M      | 5.8M    |
   | 2 clients * 12 threads  | 0.475M     | 11.4 M  |
   | 3 clients * 12 threads  | 0.467M     | 16.8M   |


<a  name="rw_fly"></a>

## RW Flying request

```bash
$make fly_client
$./fly_client -client_name val08 -threads 12
```

   |          Setup          | per-thread | peek    |
   | ----------------------- | ---------- | ------- |
   | 1 clients * 12 threads  | 2.5M       | 30M     |
   | 2 clients * 12 threads  | 2.5M       | 60M     |
   | 3 clients * 12 threads  | 2.5M       | 90M     |

<a  name="rw_or"></a>

## RW Outstanding request

```bash
$make or_client
$./or_client -client_name val08 -threads 12
```

   |          Setup          | per-thread | peek    |
   | ----------------------- | ---------- | ------- |
   | 1 clients * 12 threads  | 3.67M      | 44M     |
   | 2 clients * 12 threads  | 3.62M      | 87M     |
   | 3 clients * 12 threads  | 3.44M      | 124M    |

<a  name="rw_db"></a>

## RW Doorbell Batching

```bash
$make db_client
$./db_client -client_name val08 -threads 12
```

   |          Setup          | per-thread | peek    |
   | ----------------------- | ---------- | ------- |
   | 1 clients * 12 threads  | 5.5M       | 66M     |
   | 2 clients * 12 threads  | 5.5M       | 132M    |
   | 3 clients * 12 threads  | 3.75M      | 135M    |