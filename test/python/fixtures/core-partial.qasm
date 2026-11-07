OPENQASM 3;
include "stdgates.inc";

bit[1] c;

prx(1.0, 0.0) $1;
cz $1, $2;
c[0] = measure $5;
