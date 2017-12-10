# Overview
This sample code will crash any version of Windows 10 after 1511 (10.0.10586, TH2) up to 1703 (10.0.15063, RS2) from a process that can be run by an unprivileged user. It is thus a local denial of service vulnerability. RS3 and later versions are not affected.

# How it works
The program does not actually craft any malicious input to 'trigger an exploit'. It simply uses the function `NtCreateProcess` to create a process. This API has existed as long as NT itself, and the program does not use it improperly (though creating a functional process with an initial thread and a PEB takes about an order of magnitude more code than this). `NtCreateProcess` and its equally ancient brother `NtCreateProcessEx` were superceded by the more flexible `NtCreateUserProcess` in Windows Vista. `CreateProcess` calls `NtCreateUserProcess` under the hood.

`NtCreateUserProcess` has a wealth of so-called 'attributes' that are specified in a `PS_ATTRIBUTE_LIST` struct of variable length, depending on the number of attributes provided. (Some are accessible to `CreateProcess`: see [UpdateProcThreadAndAttribute](https://msdn.microsoft.com/en-us/library/windows/desktop/ms686880.aspx) for a heavily censored list of the possible attributes.)

Because `NtCreateUserProcess` is very different from `NtCreateProcess[Ex]`, let's take a look at the code paths they take in the kernel (`NtCreateProcess` forwards to `NtCreateProcessEx`):

**NtCreateUserProcess**
  * PspAllocateProcess
    * **PspInitializeFullProcessImageName**
  * PspAllocateThread
  * PspInsertProcess
  * PspInsertThread

**NtCreateProcessEx**
  * PspCreateProcess
    * PspAllocateProcess
      * **PspInitializeFullProcessImageName**
    * PspInsertProcess

(One interesting thing to note is that `NtCreateUserProcess` requires an initial thread, unlike `NtCreateProcess[Ex]`. Because terminating the last thread of a process will always terminate the process, this makes `NtCreateProcess[Ex]` the only way to create a Windows process with zero threads.)

The crash occurs in `PspInitializeFullProcessImageName`, which obtains the process name from the process attribute list (the first argument to the function). Wait, so what if you don't have an attribute list?

```asm
; PspInitializeFullProcessImageName
mov     [rsp+18h], rbx
mov     [rsp+20h], rsi
push    rbp
push    rdi
push    r14
lea     rbp, [rsp-70h]
sub     rsp, 170h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+60h], rax
mov     eax, [rdx+6CCh]
mov     rsi, rdx
mov     r14, rcx
test    al, 1
jnz     allocate_16bytes
mov     rax, [rcx+0B0h] ; Bzzt!
```
Then you get a null pointer dereference.

# Don't worry, it's a feature
I attempted to report this vulnerability to Microsoft on July 6, 2017. (Update: the bug had [already been found](https://bugs.chromium.org/p/project-zero/issues/detail?id=852) by Google's Project Zero a year prior to this.) I asked for contact information of someone who doesn't speak corporate BS to provide them with technical details. Microsoft was not interested and instead unironically redirected me to the [Microsoft Support Services](http://support.microsoft.com/common/international.aspx) site 'if I had any issues with Windows'.

The bug has nevertheless been fixed in RS3, no thanks to me. It's not certain whether it was fixed by accident or on purpose, because the most recent RS2 security update still has not fixed it and the vulnerability still exists on any version of Windows 10 prior to RS3 (with the exception of RTM).

So the lesson learned is: unless the exploit you found is a remote-code-execution-and-escalation-of-privilege attack, Microsoft doesn't give a shit. Unless the vulnerability is being used in the wild; then they will pretend to give a shit.
