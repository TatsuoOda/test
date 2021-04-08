#
source function.sh

F25I=1920x1080-25I-IPBBB.ts
F25P=1920x1080-25P-IPBBB.ts
H25I=1280x720-25I-IPBBB.ts
H25P=1280x720-25P-IPBBB.ts
H50P=1280x720-50P-IPBBB.ts
S25I=720x576-25I.ts
S25P=720x576-25P.ts
S50P=720x576-50P.ts

Connect ${F25I} ${F25I} 25I-dvs-25I.ts 5400 
Connect 25I-dvs-25I.ts ${F25I} 25I-dvs-25I-dvs-25I.ts 5400 
Connect 25I-dvs-25I-dvs-25I.ts ${F25I} 25I-dvs-25I-dvs-25I-dvs-25I.ts 5400

Connect ${F25I} ${F25P} 25I-25P.ts 3600  0
Connect 25I-25P.ts ${F25I} 25I-25P-25I.ts 3600 0
Connect 25I-25P-25I.ts ${F25I} 25I-25P-25I-dvs-25I.ts 5400 0
Connect 25I-25P-25I-dvs-25I.ts ${F25P} 25I-25P-25I-dvs-25I-25P.ts 3600 0
Connect 25I-25P-25I-dvs-25I-25P.ts ${F25I} 25I-25P-25I-dvs-25I-25P-25I.ts 3600

Connect 25I-25P-25I-dvs-25I.ts ${F25I} 25I-25P-25I-dvs-25I-dvs-25I.ts 5400 0
Connect 25I-25P-25I-dvs-25I-dvs-25I.ts ${F25I} 25I-25P-25I-dvs-25I-dvs-25I-dvs-25I.ts 5400 0
Connect 25I-25P-25I-dvs-25I-dvs-25I-dvs-25I.ts ${F25P} 25I-25P-25I-dvs-25I-dvs-25I-dvs-25I-25P.ts 3600 0
Connect 25I-25P-25I-dvs-25I-dvs-25I-dvs-25I-25P.ts ${F25I} 25I-25P-25I-dvs-25I-dvs-25I-dvs-25I-25P-25I.ts 3600


Connect ${S25I} ${S50P} 25I-50P.ts 3600 0
Connect 25I-50P.ts ${S25I} 25I-50P-25I.ts 3600 0
Connect 25I-50P-25I.ts ${S25I} 25I-50P-25I-dvs-25I.ts 5400 0
Connect 25I-50P-25I-dvs-25I.ts ${S50P} 25I-50P-25I-dvs-25I-50P.ts 3600 0
Connect 25I-50P-25I-dvs-25I-50P.ts ${S25I} 25I-50P-25I-dvs-25I-50P-25I.ts 3600

Connect ${S25P} ${S50P} 25P-50P.ts 3600 0
Connect 25P-50P.ts ${S25P} 25P-50P-25P.ts 3600 0
Connect 25P-50P-25P.ts ${S25P} 25P-50P-25P-dvs-25P.ts 5400 0
Connect 25P-50P-25P-dvs-25P.ts ${S50P} 25P-50P-25P-dvs-25P-50P.ts 3600 0
Connect 25P-50P-25P-dvs-25P-50P.ts ${S25P} 25P-50P-25P-dvs-25P-50P-25P.ts 3600
