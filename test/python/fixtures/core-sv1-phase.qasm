OPENQASM 3;
include "stdgates.inc";

bit[4] c;

gphase(-1.5707963267948966);
rz(-1.5707963267948966) $0;
rx(-3.1415926535897931) $0;
rz(-1.5707963267948966) $0;
cnot $0, $1;
c[0] = measure $0;
c[1] = measure $1;
c[2] = measure $2;
c[3] = measure $3;
