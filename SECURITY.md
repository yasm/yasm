Yasm Security policy
====================

A security bug is one that compromises the security of a system by either
making it unavailable, compromising the integrity of the data or by providing
unauthorized access to resources on the system through untrusted inputs.  In
the context of yasm, it is expected that all inputs are trusted, since it is
developer code.  It is the responsibility of the developer to either verify the
authenticity of the code they're building or to build untrusted code in a
sandbox to protect the system from any ill effects.  This responsibility also
extends to the libyasm library.  While the library aims to be robust and will
fix bugs arising from bogus inputs, it is the responsibility of the application
to ensure that either the environment under which the call is made is isolated
or that the input is sanitized.

As such, all bugs will be deemed to have no security consequence with the
exception of bugs where yasm generates code that invoke [undefined behaviour in
a
system](https://www.cs.cmu.edu/~rdriley/487/papers/Thompson_1984_ReflectionsonTrustingTrust.pdf)
from valid, safe and trusted assembly code.

Reporting security bugs
-----------------------

To report security issues privately, you may reach out to one of the members of
the [Yasm Team](https://github.com/yasm/yasm/wiki/Yasmteam).  Most issues
should just go into GitHub issues as regular bugs.
