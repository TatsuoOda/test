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

# 25P��dvs���Ѳ��Ϥɤ��ʤ�(+n/2*V)
Connect ${F25P} ${F25P} 25P-D-25P.ts 4500 
Connect 25P-D-25P.ts ${F25P} 25P-D-25P-D-25P.ts 4500 
Connect 25P-D-25P-D-25P.ts ${F25P} 25P-D-25P-D-25P-D-25P.ts 4500 
Connect 25P-D-25P-D-25P-D-25P.ts ${F25P} 25P-D-25P-D-25P-D-25P-D-25P.ts 4500 

# 25P��dvs���Ѳ��Ϥɤ��ʤ�(+n*V)
Connect ${F25P} ${F25P} 25P-d-25P.ts 5400 
Connect 25P-d-25P.ts ${F25P} 25P-d-25P-d-25P.ts 5400 
Connect 25P-d-25P-d-25P.ts ${F25P} 25P-d-25P-d-25P-d-25P.ts 5400 
Connect 25P-d-25P-d-25P-d-25P.ts ${F25P} 25P-d-25P-d-25P-d-25P-d-25P.ts 5400 


