ticket_spinlock
===============

A user space implementation of ticket spinlocks similar to the one found in the linux kernel. 
Its simple to understand and use besides ensuring that spinlocks are acquired in the order in which they arrive instead of being random.

The concept is similar to a ticketing system. It has a head and a tail ticket number

The tail indicates the ticket number granted to the thread trying to own the lock.
The head indicates the current ticket number being processed in the line.

A new thread desiring to own the lock atomically reads the current tail or its own position while incrementing the ticket/tail.

If the head matches the tail, then the lock can be taken by the thread.
In case of a mismatch, the thread spins waiting for the head to match its tail or its position in the line.

To unlock, the thread just increments the head position in the lock to indicate the next turn that would be grabbed based on the thread that matches the head with its allotted tail position.

This mechanism ensures lock fairness without resorting to randomness which would result if we have all the threads spinning for a global lock to get to 1 or unlocked state.
