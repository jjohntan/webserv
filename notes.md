Flags notes

events |= POLLOUT;
events = events | POLLOUT;

	Bitwise OR “adds” the flag without removing others:
	If events was POLLIN (say binary 001),
	and POLLOUT is 100,
	then events |= POLLOUT makes it 101 (now watching for both read and write).

events &= ~POLLOUT;
~POLLOUT flips the bits of POLLOUT

	keep all bits that are 1 in the mask (i.e., all bits except POLLOUT),
	force the POLLOUT bit to 0.

		Suppose POLLIN = 0b0001, POLLOUT = 0b0100.
		Current events = 0b0101 (POLLIN + POLLOUT).
		~POLLOUT = 0b1011.
		events & ~POLLOUT = 0b0101 & 0b1011 = 0b0001 → POLLOUT cleared, POLLIN kept.

Mental model (cheat sheet)

	Enable a flag: events |= FLAG; // set that bit to 1, keep others
	Disable a flag: events &= ~FLAG; // set that bit to 0, keep others
	Toggle a flag: events ^= FLAG; // flip that bit 