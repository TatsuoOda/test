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
# 50P-25I 
#-----------
Connect ${S50P} ${S25I} 50P-25I.ts 3600
Connect 50P-25I.ts ${S50P} 50P-25I-50P.ts 3600
Connect 50P-25I-50P.ts ${S50P} 50P-25I-50P-d-50P.ts 1800
Connect 50P-25I-50P-d-50P.ts ${S25I} 50P-25I-50P-d-50P-25I.ts 3600

#-----------
# 25I-50P : 50P-25I
#-----------
Connect ${S25I} ${S50P} 25I-50P.ts 3600
Connect 25I-50P.ts ${S25I} 25I-50P-25I.ts 3600

Connect 25I-50P-25I.ts ${S50P} 25I-50P-25I-50P.ts 3600
Connect 25I-50P-25I-50P.ts ${S25I} 25I-50P-25I-50P-25I.ts 3600
Connect 25I-50P-25I-50P-25I.ts 25I-50P-25I.ts 25I-50P-25I-50P-25I-s-25I-50P-25I.ts 1800
#
#A=25I-50P
#Connect ${S25I} ${S50P} ${A}.ts 3600
#Connect ${A}.ts ${S25I} ${A}-25I.ts 3600
#B=${A}-25I
#Connect ${B}.ts ${S50P} ${B}-50P.ts 3600
#C=${A}-50P
#Connect ${C}.ts ${S25I} ${C}-25I.ts 3600
#D=${A}-25I
#Connect ${D}.ts ${B}.ts ${D}-s-${B}.ts 1800

#-----------
Connect ${S25I} ${S50P} 25I-s-50P.ts 1800
Connect 25I-s-50P.ts ${S25I} 25I-s-50P-25I.ts 3600

Connect 25I-s-50P-25I.ts ${S50P} 25I-s-50P-25I-s-50P.ts 1800
Connect 25I-s-50P-25I-s-50P.ts ${S25I} 25I-s-50P-25I-s-50P-25I.ts 3600
Connect 25I-s-50P-25I-s-50P-25I.ts 25I-s-50P-25I.ts 25I-s-50P-25I-s-50P-25I-s-25I-s-50P-25I.ts 1800


# 25I-50P : 50P-25I
#-----------
Connect ${F25I} ${H50P} F25I-50P.ts 3600
Connect F25I-50P.ts ${F25I} F25I-50P-F25I.ts 3600

Connect F25I-50P-F25I.ts ${H50P} F25I-50P-F25I-50P.ts 3600
Connect F25I-50P-F25I-50P.ts ${F25I} F25I-50P-F25I-50P-F25I.ts 3600
Connect F25I-50P-F25I-50P-F25I.ts F25I-50P-F25I.ts F25I-50P-F25I-50P-F25I-s-F25I-50P-F25I.ts 1800
#

#-----------
Connect ${F25I} ${H50P} F25I-s-50P.ts 1800
Connect F25I-s-50P.ts ${F25I} F25I-s-50P-F25I.ts 3600

Connect F25I-s-50P-F25I.ts ${H50P} F25I-s-50P-F25I-s-50P.ts 1800
Connect F25I-s-50P-F25I-s-50P.ts ${F25I} F25I-s-50P-F25I-s-50P-F25I.ts 3600
Connect F25I-s-50P-F25I-s-50P-F25I.ts F25I-s-50P-F25I.ts F25I-s-50P-F25I-s-50P-F25I-s-F25I-s-50P-F25I.ts 1800

