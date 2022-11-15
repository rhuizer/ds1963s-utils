# ds1963s-utils

A collection of utilities and support libraries to communicate with, emulate,
and side-channel the ds1963s ibutton.

## ds1963s-tool

This is a command line utility that implements complex sequences of commands
to perform common tasks.  It can be used to dump all information of a dongle,
including the secrets into a descriptive text report or a yaml file.

The secrets can be dumped using a pure software side-channel attack based on
partial key overwrites.

## ds1963s-shell

Is a low-level shell utility that can be used to execute low-level commands
on the ds1963s button.  This utility is useful to experiment with the dongle
and to discover how it reacts to certain commands.  It is interactive, as
many of the low-level commands are useless by themselves.  For complex
functionality ds1963s-tool should be used instead.

### Example session

We provide an example ds1963s-shell session below.

We first fill the scratchpad with 32 0x41 bytes and read the scratchpad.  The
scratchpad read command will dump a lot of information from the dongle.
```
ds1963s> esp
ds1963s> wsp 0 4141414141414141414141414141414141414141414141414141414141414141
ds1963s> rsp
Address: 0x0000  (Page   : 0)
TA1    : 0x00    (T07:T00: 0, 0, 0, 0, 0, 0, 0, 0)
TA2    : 0x00    (T15:T08: 0, 0, 0, 0, 0, 0, 0, 0)
E/S    : 0x1f
E      : 0x1f    (E04:E00: 1, 1, 1, 1, 1)
PF     : 0
AA     : 0
CRC16  : 0x5ecc  (OK)
Offset : 0       (T04:T00: 0, 0, 0, 0, 0)
Length : 32     
Data   : 4141414141414141414141414141414141414141414141414141414141414141
```

Now we read 0 bytes of memory at address 0x1f.  This is a simple way to latch
in the TA1 and TA2 registers without side effects as can be seen from the
scratchpad read command that follows.
```
ds1963s> rm 0x1f 0
ds1963s> rsp
Address: 0x001f  (Page   : 0)
TA1    : 0x1f    (T07:T00: 0, 0, 0, 1, 1, 1, 1, 1)
TA2    : 0x00    (T15:T08: 0, 0, 0, 0, 0, 0, 0, 0)
E/S    : 0x1f
E      : 0x1f    (E04:E00: 1, 1, 1, 1, 1)
PF     : 0
AA     : 0
CRC16  : 0xd7cc  (OK)
Offset : 31      (T04:T00: 1, 1, 1, 1, 1)
Length : 1      
Data   : 41
```

We execute the scratchpad copy command.  The authorization pattern needs to
match 'Address' and 'E/S' above.  We can see from the scratchpad read that the
AA flag is now set, so the copy succeeded.
```
ds1963s> csp 0x1f 0x1f
ds1963s> rsp
Address: 0x001f  (Page   : 0)
TA1    : 0x1f    (T07:T00: 0, 0, 0, 1, 1, 1, 1, 1)
TA2    : 0x00    (T15:T08: 0, 0, 0, 0, 0, 0, 0, 0)
E/S    : 0x9f
E      : 0x1f    (E04:E00: 1, 1, 1, 1, 1)
PF     : 0
AA     : 1
CRC16  : 0xb60c  (OK)
Offset : 31      (T04:T00: 1, 1, 1, 1, 1)
Length : 1      
Data   : 41
```

Finally we dump 64 bytes of memory, and see the final byte of the first page
has been set to 0x41 as we expected.
```
ds1963s> rm 0 64
00000000: ffff ffff ffff ffff ffff ffff ffff ffff  ................
00000010: ffff ffff ffff ffff ffff ffff ffff ff41  ...............A
00000020: aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa  ................
00000030: aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa  ................
```

## ds1963s-emulator

An emulator for the ds1963s ibutton.  Documentation to be provided.
