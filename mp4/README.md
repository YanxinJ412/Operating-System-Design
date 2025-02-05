[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/j-mH633X)

# UIUC CS 423 MP4

Your Name: Yanxin Jiang

Your NetID: yanxinj2

## Overview

In this mp, we'll implement a Rex kernel extension program that performs error injections to system calls, which is an important technique of testing the interaction between kernel and user-space. 

## implementation

To implement a error injection mechanism, we need to override the return value of the target system call. 

**Rex Program**: It hooks to the entry of the target system call via kprobe and use the kprobe-override-return mechanism to set the return value to the target errno.
**Loader**: It is the user-app loader program that loads the Rex program into the kernel, attaches it to the target system call using kprobe, and then fork child to run your userapp.
**user-app**: It invokes some system call, and calls perror upon errors from the system call.

### Rex Program

It will create two maps: one is target pid map and the other is target errno map. Since we don't need to consider multihtreading, each map should only contain one item so the default key of these targets should be 0. We use `bpf_map_lookup_elem` to look up the target pid and target errno. Use `bpf_get_current_task` to get the current pid and compare with the target pid. If mismatch, early return. Else, perform the error injection using `bpf_override_return` so that the injected errno should be the return value. Since we need to inject negative value, use `-(errno as i64) as u64` to cast the errno for injection.

### Loader
Thhe loader program should input two values: syscall and errno. It load the obbject file of Rex program, find and attach the bpf program to the target system call. Fork a child to run user_app so that even if the user_app finishes the execution, bpf is still attached. Update the errno and the pid to the eBPF map using `bpf_map_update_elem` in child process and run `user_app`. The parent should wait until the child is finished.

### User_app:
Simply call some system calls and invoke perror to print the error msg associated with the injected errno. 

# MP4: Rex kernel extensions

**Note: You need to be on the x86-64 architecture in order to work on this MP.
We assume the x86-64 architecture and ABI in this writeup. If you are using
aarch64 architecture, you will need to find another x86-64 computer or run this
project in emulation**

**Please make sure you read through this document at least once before
starting.**

# Table of Contents

- [Introduction](#introduction)
- [Problem Description](#problem-description)
- [Environtment setup](#environtment-setup)
  - [Repo setup](#repo-setup)
  - [Build Linux kernel and libbpf](#build-linux-kernel-and-libbpf)
  - [Run the `hello` sample program](#run-the-hello-sample-program)
- [Understand the repo structure](#understand-the-repo-structure)
- [Implementation](#implementation)
  - [Rex kprobe program](#rex-kprobe-program)
  - [Loader program](#loader-program)
  - [Userapp](#userapp)
- [Other Requirements](#other-requirements)
- [Extra credit](#extra-credit)
- [Resources](#resources)

# Introduction

The emergence of verified eBPF bytecode is ushering in a new era of safe kernel
extensions. In this paper, we argue that eBPF’s verifier—the source of its
safety guarantees—has become a liability. In addition to the well-known bugs
and vulnerabilities stemming from the complexity and ad hoc nature of the
in-kernel verifier, we highlight a concerning trend in which escape hatches to
unsafe kernel functions (in the form of helper functions) are being introduced
to bypass verifier-imposed limitations on expressiveness, unfortunately also
bypassing its safety guarantees. We propose safe kernel extension frameworks
using a balance of not just static but also lightweight runtime techniques. We
describe a design centered around kernel extensions in safe Rust that will
eliminate the need of the in-kernel verifier, improve expressiveness, allow for
reduced escape hatches, and ultimately improve the safety of kernel extensions.

The basic ideas are documented in [this workshop
paper](https://jinghao-jia.github.io/papers/hotos23-untenable.pdf) (no need to
read through).

# Problem Description

Your task is to implement a Rex kernel extension program that performs error
injections to system calls, which is an important technique of testing the
interaction between kernel and user-space. This is conceptually
straightforward. However, the real challenge lies in mastering Rex operations
and integrating them with your knowledge of Linux kernel programming, Rust
programming, and other essential aspects like ELF (Executable and Linkable
Format). This task will test your technical skills and ability to quickly adapt
to new programming environments.

To implement a error injection mechanism, your objective is to override the
return value of the target system call. The steps you need to follow are:

1. Write a Rex program that hooks to the entry of the target system call via
   kprobe and use the kprobe-override-return mechanism to set the return value
   to the target errno.
2. Create a user-space loader program that loads your Rex program into the
   kernel, attaches it to the target system call using kprobe, and then fork
   child to run your userapp.
3. Create a user-app that invokes some system call, and calls `perror` upon
   errors from the system call.

# Environment setup

This MP has special requirements on the development environment.

## Nix setup

This MP is using Nix, a package manager could allow you to resolve these dependency
requirements.

Check out the <https://nixos.org/download/> for installation
instructions, the single-user installation should be enough.

## Repo setup

Clone your MP4 repo and enter Nix env:

```bash
git clone <mp4_repo_url>
pushd <mp4_repo>
nix develop --extra-experimental-features nix-command --extra-experimental-features flakes
popd
```

And then clone the MP4 resources

```bash
git clone git@github.com:uiuc-cs423-fall24/MP4-Resources.git
pushd MP4-Resources
git lfs install --local
git lfs pull
popd
```

Install the kernel source and rust artifacts from the resource repo:

```bash
MP4-Resources/install.sh <mp4_repo_dir>
```

This creates the `linux` and `rust` directory under your mp4 directory and
copies the kernel source and rust artifacts accordingly.

## Build Linux kernel and libbpf

Confirm to re-enter the Nix environment.

```bash
exit # exit the current Nix
pushd <mp4_repo>
nix develop --extra-experimental-features nix-command --extra-experimental-features flakes
popd
```

The linux directory should already have a `.config` file. We need to build the
kernel and the `libbpf` loader library.

```bash
pushd <mp4_repo_dir>/linux
make olddefconfig
make -kj`nproc`
make -kj`nproc` -C tools/lib/bpf
popd
```

## Run the `hello` sample program

```bash
# build librex
make -C <mp4_repo_dir>/librex

# build the hello sample
make -C <mp4_repo_dir>/samples/hello/

# enter qemu
cd <mp4_repo_dir>/linux
<qscript_dir>/cs423-q

# Inside qemu, move to the hello directory and run the sample
cd ../samples/hello
./loader &
./event-trigger
```

and you should see some similar output as the following:

```console
<...>-245     [002] d...1    18.417331: bpf_trace_printk: Rust triggered from PID 245.
```

We recommend you to take a look at both the hello program and the loader file
to get familiar with the task you are going to work on.

# Understand the repo structure

The repository contains the following directories:

- `librex`: This is the loader library for Rex programs built on top of
  `libbpf`. You should not modify any files in this directory. The
  [`README`](librex/README.md) documents the APIs.
- `rex`: This is the kernel crate for Rex programs and contains the program
  type and helper function definitions on top of the kernel FFI.
- `rex-macros`: The `proc-macro` crate for Rex -- used for specifying the
  program type and attachment meta information. All macros in this crate are
  re-exported by `rex`.
- `samples/error_injection`: This directory contains the rex program that you
  need to implement.
  - Specifically, you should place the Rex program code in `src/main.rs`, the
    loader code in `loader.c`, and your userapp code in `userapp.c`.
- `samples/hello`: This directory contains the `hello` example that we just saw.
- `samples/map_test`: Another sample program demonstrating the use of eBPF maps.
- `samples/tracex5`: A sample Rex kprobe program that attaches to the
  `__seccomp_filter()` function in the kernel.

# Implementation

## Rex kprobe program

We will use the kprobe hook point of Rex and the kprobe-override-return
mechanism to implement our error injector.
[Kprobe](https://docs.kernel.org/trace/kprobes.html) allows instrumentation on
almost any instructions in the kernel. Our program should be attached to the
entry (i.e. first instruction) of the target system call. As kprobe also allows
modifying the contexts it instruments, we can leverage this feature to override
the return value and force an early return, which involves setting the return
value register (`%rax`) and the instruction pointer (`%rip`).
[`samples/tracex5`](samples/tracex5) is a sample kprobe program for your
reference.

Since neither the errno to inject nor the process to inject the errno to
(globally injecting errors to system calls could lead to unexpect result) is
known at build time, we need a mechanism to pass this information to the
program at runtime. To this end, we use eBPF maps to pass information between
user-space and the program in the kernel. Your loader will update such
information to the map and your program will get the errno value and the target
process from the maps.

We have already provided you the skeleton code:

```rust
#[rex_kprobe]
pub fn err_injector(obj: &kprobe, ctx: &mut PtRegs) -> Result {
    todo!("implement your error injection logic here");
}
```

The `rex-kprobe` macro marks the `err_injector()` function as the entry of your
kprobe program (think about it as `main()` in C) and all of your program logic
goes into this function (or other functions it calls).

The implementation can be divded into the following steps:

1. Define the maps to store your errno value and the target PID. Your program
   should read these values out from the maps. You may find the
   [`samples/map_test`](samples/map_test) sample useful for map-related APIs.
2. Compare the PID of the current process with your target PID, if they do not
   match, then do an early return. Hint: check the `rex::kprobe::kprobe` API to
   see how to obtain the current PID.
3. Perform the error injection. Again, explore the `rex::kprobe::kprobe` API to
   see how you can do this. Make sure the errno you inject is the negative
   value of its definition (e.g. `EINVAL` is defined to be `22`, you need to
   inject `-22`).

## Loader program

The loader program is responsible for loading, attaching, and updating the maps
for our Rex program. Your loader program should take in the target system call
name as a string followed by the errno as a positive number (as errnos are
defined to be all positive numbers):

```console
./loader <syscall> <errno>
# An example invocation to inject EINVAL (22) to dup
./loader dup 22
```

Your loader program should contain the following steps:
1. Load the Rex program -- check out other samples and librex API on how to do
   this.
2. Attach the program to the system call entry specified on the command line
   (check out the `bpf_program__attach_ksyscall()` function from `libbpf`).
3. Update the errno to the eBPF map using libbpf APIs. Again, you may find the
   [`samples/map_test`](samples/map_test) sample useful for map-related APIs.
4. Fork a child: The child should set update its PID to the map and then `exec`
   into your userapp so that your userapp will get the injected error. The
   parent should wait for the child to finish.

## Userapp

Your userapp should just call some system calls and invoke `perror` to print
the error msg associated with the injected errno (if any).

# Other Requirements

- Do not change any other files except the files mentioned above.
- Your Rust code should not have any `unsafe` block in the Rex program.
- You should not use any other extern crates (i.e. Rust packages) other than
  the ones already specified in `Cargo.toml`.
- You cannot use the Rust `std` library because it is not available in
  standalong mode, but the `core` library remains largely available.
- This research is currently not available to the public. So please do not put
  any Rex code on ChatGPT (or other chatbots).

# Extra credit

**Note: Please attempt the extra credit only after you have finished the error
injector, as this part of the MP is much more challenging.**

Since Rust implements many safety checks at runtime using panics (the same as
C++ exceptions), it is important to handle the Rust panics during program
execution and still leave the kernel in a consistent state. For the extra
credit, you will complete our custom Rust exception handler in Rex.

To correctly handle the expection, 2 tasks must be done:
- Clean up any allocated resources
- The execution needs to get back to the normal control flow after the program
  and the stack should have the same layout as it had before the program
  invocation.

The panic handler in the `rex` crate have already done the first task. You are
asked to copmlete this second task for the extra credit.

Since this is the extra credit, we won't discuss too much on how this should be
done -- please feel free to be creative and implement you own way of handling
exceptional control flows.  The basic idea, though, is to use a series of
control flow transfers (e.g., calls and jumps) to redirect the exceptional
control flow back to the normal control flow and adjust the stack pointer to
the same location it was at before program invocation.

We have left some useful skeletons for you in the kernel source tree:
- `rex_landingpad()` in `arch/x86/net/rex.c` is the point where exception
  control flow re-enters the kernel (as it is called by the panic handler in
  Rex). Right now it calls kernel panic, but you should further redirect the
  control flow so that it eventually joins the normal execution path.
- `rex_dispatcher_func()` in `arch/x86/net/rex_64.S` is the function that
  invokes your Rex program. This function is written entirely in GAS style
  assembly. Due to its low
  level nature, this is a great place to modify the registers and joining the
  control flows.

Some other tips:
- GAS style assembly allows header includes and macro expansions, check other
  assembly files, e.g. `arch/x86/kernel/ftrace_64.S` on how to use them.
- If your implementation ever needs to store some information during program
  execution, you can consider using a [per-CPU
  storage](https://0xax.gitbooks.io/linux-insides/content/Concepts/linux-cpu-1.html)
  to avoid locks that are generally required for shared globals as well as the
  allocation overhead needed by heaps. This is possible in our case because Rex
  programs are pinned to a single CPU and you can assume one Rex program will
  not be preempted by another, which means the per-CPU storage is not shared in
  anyway.

For the submission of this part, please commit your modification to the kernel
as a patch into your MP4 repo.

# Resources

We recommend you to get your hands dirty directly and check these resources on
demand. In fact, we didn't know Rust well when we started this project – you
can always learn a language by writing the code.

- eBPF: [ebpf.io](https://ebpf.io/) and [The Illustrated Children’s Guide to
  eBPF](https://ebpf.io/books/buzzing-across-space-illustrated-childrens-guide-to-ebpf.pdf).
  are both good places to start. You can also find the official kernel
  documentation
  [here](https://elixir.bootlin.com/linux/v6.11/source/Documentation/bpf) along
  with the source code. In particular, try answering:
  - What is eBPF?
  - What is kprobe?
  - What are some example use cases of eBPF?
  - How are eBPF programs loaded to the kernel and bind XDP program to
    interfaces?
  - How are the execution of eBPF programs triggered?
  - What are eBPF helpers?
- Rust: If you are not familiar with the Rust program language, we have some
  resources for you:
  - [The Rust book](https://doc.rust-lang.org/book/) (Probably the most
    comprehensive guide on Rust programming)
  - [Library API reference](https://doc.rust-lang.org/core/index.html) (for
    searching API specifications)
  - [The Rust playground](https://play.rust-lang.org) (for trying out programs)
