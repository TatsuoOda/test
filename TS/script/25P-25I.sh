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

#-----------
# 25P-25I (Resize)
#-----------
if [ x$1 = xx ]
then
Connect ${F25P} ${H25I} F25P-25I.ts 3600
Connect F25P-25I.ts ${F25P} F25P-25I-F25P.ts 3600
Connect F25P-25I-F25P.ts ${H25I} F25P-25I-F25P-25I.ts 3600
Connect F25P-25I-F25P-25I.ts ${F25P} F25P-25I-F25P-25I-F25P.ts 3600

UNIT5=F25P-25I-F25P-25I-F25P
Connect ${UNIT5}.ts ${UNIT5}.ts ${UNIT5}-s-${UNIT5}.ts 1800
Connect ${UNIT5}-s-${UNIT5}.ts ${UNIT5}.ts ${UNIT5}-s-${UNIT5}-s-${UNIT5}.ts 1800

Connect ${UNIT5}.ts ${UNIT5}.ts ${UNIT5}-d-${UNIT5}.ts 5400
Connect ${UNIT5}-d-${UNIT5}.ts ${UNIT5}.ts ${UNIT5}-d-${UNIT5}-d-${UNIT5}.ts 5400
fi

#-----------
# 25P-25I (No-Resize)
#-----------
Connect ${F25P} ${F25I} 25P-25I.ts 3600
Connect 25P-25I.ts ${F25P} 25P-25I-25P.ts 3600
Connect 25P-25I-25P.ts ${F25I} 25P-25I-25P-25I.ts 3600
Connect 25P-25I-25P-25I.ts ${F25P} 25P-25I-25P-25I-25P.ts 3600

UNIT5=25P-25I-25P-25I-25P
Connect ${UNIT5}.ts ${UNIT5}.ts ${UNIT5}-s-${UNIT5}.ts 1800
Connect ${UNIT5}-s-${UNIT5}.ts ${UNIT5}.ts ${UNIT5}-s-${UNIT5}-s-${UNIT5}.ts 1800

Connect ${UNIT5}.ts ${UNIT5}.ts ${UNIT5}-d-${UNIT5}.ts 5400
Connect ${UNIT5}-d-${UNIT5}.ts ${UNIT5}.ts ${UNIT5}-d-${UNIT5}-d-${UNIT5}.ts 5400

if [ x$1 = xTest ]
then
Connect -n F25P-25I-F25P-25I-F25P.ts ${H25I} F25P-25I-F25P-25I-F25P-25I.ts 3600
Connect F25P-25I-F25P-25I-F25P-25I.ts ${F25P} F25P-25I-F25P-25I-F25P-25I-F25P.ts 3600
UNIT7=F25P-25I-F25P-25I-F25P-25I-F25P
Connect ${UNIT7}.ts ${UNIT7}.ts ${UNIT7}-s-${UNIT7}.ts 1800
Connect ${UNIT7}-s-${UNIT7}.ts ${UNIT7}-s-${UNIT7}.ts ${UNIT7}-s-${UNIT7}-s-${UNIT7}-s-${UNIT7}.ts 1800

Connect -n F25P-25I-F25P-25I-F25P-25I-F25P.ts ${H25I} F25P-25I-F25P-25I-F25P-25I-F25P-25I.ts 3600
Connect  F25P-25I-F25P-25I-F25P-25I-F25P-25I.ts ${F25P} F25P-25I-F25P-25I-F25P-25I-F25P-25I-F25P.ts 3600
UNIT9=F25P-25I-F25P-25I-F25P-25I-F25P-25I-F25P

Connect ${UNIT9}.ts ${UNIT9}.ts ${UNIT9}-s-${UNIT9}.ts 1800
#Connect ${UNIT9}-s-${UNIT9}.ts ${UNIT9}-s-${UNIT9}.ts ${UNIT9}-s-${UNIT9}-s-${UNIT9}-s-${UNIT9}-s-${UNIT9}.ts 1800
fi
