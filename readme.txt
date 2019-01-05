====  rlimit_rss  ====

rlimit_rss is an EXPERIMENTAL Linux kernel module that enforces
RLIMIT_RSS.  When the RSS of a process exceeds its RLIMIT_RSS (hard or
soft), the module will kill the process with a SIGKILL signal.

I developed rlimit_rss on Ubuntu 18.04 with a 4.15 Linux kernel.
rlimit_rss may (or may not) compile and work with earlier kernel
versions.  My primary use case was limiting the amout of memory
JavaScript could consume in any given tab in the Chromium web browser.

WARNING: rlimit_rss is the first kernel module I have ever written.  I
am not a Linux kernel expert.  rlimit_rss was cobbled together based
on information that I scavenged from DuckDuckGo and Google searches.
Surprisingly, rlimit_rss has never crashed my system, even during
development.  However, rlimit_rss may crash your system and delete all
your data!!  Use at your own risk!!

Every 10 ticks, rlimit_rss checks the (hard and soft) RLIMIT_RSS of
each process.  If the (hard or soft) limit of a process is less than
RLIM_INFINITY, then rlimit_rss calcualetes the RSS usage of that
process.  If the usage exceeds the limit, then rlimit_rss sends a
SIGKILL signal to the process.

Consequently, the RSS of a process may temporarily (for up to 10
ticks) exceed the limit.  It might be possible to modify the
rlimit_rss module so that it hooks directly into the process scheduler
or memory manager.  However, such hooking would probably be
significantly more complicated to implement than simple, periodic
polling that is currently implemented.

Every 10 ticks, the rlimit_rss module will scan the RSS usage of every
process.  If you have thousands (or more) of processes on your system,
the performance cost of these periodic scans might be significant.  I
have only used rlimit_rss on systems with hundereds processes.


====  Installation  ====

$  git  clone  https://github.com/parke/rlimit_rss.git
$  cd  rlimit_rss
$  make
$  sudo  insmod  rlimit_rss.ko


====  Testing  ====

$  bash
$  ulimit  -a  |  grep  'max memory size'
max memory size         (kbytes, -m) unlimited
$  ulimit  -m  10000
$  ulimit  -a  |  grep  'max memory size'
max memory size         (kbytes, -m) 10000
$  python3  -q
>>> s  =  'x' *  1_000_000
>>> s  =  'x' * 10_000_000
>>> Killed


====  Bug reports  ====

Please submit bug reparts on GitHub.


====  Logging  ====

When rlimit_rss is loaded into the kernel, the following entries will
be written in /var/log/kern.log:

[snip] rlimit_rss  init
[snip] rlimit_rss  tasks 140  hard 1  soft 1
[snip] rlimit_rss  found  pid 21846  rss 4702208  hard 10240000  soft 10240000

"init" means the module is loading.

"tasks 140  hard 1  soft 1" means:
140 tasks/processes are currently running.
1 process has a hard limit less than RLIM_INFINITY.
1 process has a soft limit lest than RLIM_INFINITY.

A "found" line will be logged for each process whose hard or soft
RLIMIT_RSS is less than RLIM_INFINITY.  Each found line displays the
pid, the current RSS usage, and the current hard and soft RSS limits
of the process.

In the above example, 140 processes are running.  1 process (out of
the 140) has a hard limit less than RLIM_INFINITY.  1 process (out of
the 140) has a soft limit less than RLIM_INFINITY.

When a process's RSS usages exceeds its limit, the following entry
will be written in /var/log/kern.log :

[snip] rlimit_rss  kill  pid 21863  rss 21168128  hard 10240000  soft 10240000

When rlimit_rss is unloaded, the following entries will be written in
/var/log/kern.log:

[snip] tasks 139  hard 0  soft 0
[snip] exit

In the above example, at the time rlimit_rss was removed from the
kernel, all 139 processes had hard and soft RLIMIT_RSS of
RLIM_INFINITY.


====  Commentary  ====

As can be seen in the below history section, it appears
that RLIMIT_RSS is a non-standard, now obsolete, and now unenforced
limit.  It was once used (at least on old versions of Linux and
FreeBSD) to guide the memory manager in deciding which processes to
swap to disk when memory became scarce.

While RLIMIT_RSS is currently totally unused, the RLIMIT_RSS
infrastructre is still present, and it is operational (other than
lacking enforcement).  So... I decided to use the existing RLIMIT_RSS
infrastructure to actually enforce limits on process memory usage.

(Aside:  In systems with swap, it might not make sense to enforce a
hard RSS limit.  The limit could be exceeded when the memory manager
moves portions of the process from swap back into RAM.  Some
users/administrators might find it strange that a process would be
killed because of actions taken by the memory manager.  However, I
disable swap entirely on my systems.)

My primary use case for the rlimit_rss kernel module is to limit the
amount of memory that can be consumed by JavaScript in a browser tab
in the Chromium web browser.  I had been using RLIMIT_AS and
RLIMIT_DATA to try to achieve the same result, but both _AS and _DATA
are imprecise limits, at least when you are trying to limit actual
memory usage by JavaScript.


====  Partial History of RLIMIT_RSS  ====

The Linux manpage for setrlimit says:

  RLIMIT_RSS
    This  is  a  limit (in bytes) on the process's resident set (the
    number of virtual pages resident in RAM).  This limit has effect
    only  in  Linux  2.4.x,  x < 30, and there affects only calls to
    madvise(2) specifying MADV_WILLNEED.

The FreeBSD manpage for setrlimit says:

  RLIMIT_RSS
    When there	is memory pressure and swap is available, pri-
    oritize eviction of a process' resident pages beyond this
    amount (in	bytes).	 When memory is	not under pressure,
    this rlimit is effectively	ignored.  Even when there is
    memory pressure, the amount of available swap space and
    some sysctl settings like vm.swap_enabled and
    vm.swap_idle_enabled can affect what happens to processes
    that have exceeded	this size.

    Processes that exceed their set RLIMIT_RSS	are not	sig-
    nalled or halted.	The limit is merely a hint to the VM
    daemon to prefer to deactivate pages from processes that
    have exceeded their set RLIMIT_RSS.

The OpenBSD manpage for setrlimit says:

  RLIMIT_RSS
    The maximum size (in bytes) to which a process's resident
    set size may grow. This setting is no longer enforced, but
    retained for compatibility.

POSIX (The Open Group Base Specifications Issue 7, 2018 edition IEEE
Std 1003.1-2017 [Revision of IEEE Std 1003.1-2008]) specifies
getrlimit and setrlimit, but does not mention RLIMIT_RSS.

The Illumos (OpenIndiana/OpenSolaris?) setrlimit man page (dated
August 21, 2006) does not mention RLIMIT_RSS.
