OPENQASM 3;
include "stdgates.inc";

bit[4] c;
c = "0000";

prx(1.0, 0.0) $1;
cz $1, $2;
