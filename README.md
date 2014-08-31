idainject
=========

http://newgre.net/idainject

This plugin allows you to inject dlls into a debugged process, either prior to process creation or when the debugger is attached. The injected dll can then do some fancy stuff inside the debugged process.
To realize dll injection before process creation, new import descriptors are added to the image import directory of the debuggee, whereas injection into an already running process is realized via shellcode injection, which in turn loads the dll in question.
