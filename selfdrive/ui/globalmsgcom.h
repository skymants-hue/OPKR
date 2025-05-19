#pragma once
inline float msgcom[850]= { 0 };
/*
0	spare
1	brakePress
2	gasPress
3	brakeHold
4	cpuPerc
5	cpuTemp
6	ambientTemp
7	fanSpeed
8	car_speed
9	leftblindspot
10	rightblindspot
11	leftBlinker
12	rightBlinker
13	angleSteers
14	gap_by_speed_on
15	brakeLights
16	steerOverride
17	tpmsUnit
18	  float tpmsPressureFl 
19	  float tpmsPressureFr 
20	  float tpmsPressureRl 
21	float tpmsPressureRr;  
22	float radarDistance;  
23	int32 limitSpeedCamera;  // 기본값 0
24	float limitSpeedCameraDist;  // 기본값 0
25	int32 mapSign;  
26	int32 mapSignCam;  
27	float vSetDis;  
28	bool cruiseAccStatus;  
29	bool driverAcc;  
30	int32 laneless_mode;  
31	int32 top_text_view;  
32	bool is_speed_over_limit;  
33	bool controlAllowed;  
34	bool steer_warning;  
35	bool stand_still;  
36	bool show_error;  
37	int32 display_maxspeed_time;  // 기본값 0
38	bool speedlimit_signtype;  
39	float engine_rpm;  
40	float ctrl_speed;  
41	float maxspeed;
42	getGearShifter;
43	cruise_gap;
44	dm_active;
45	getSeatbeltUnlatched;
46	
47	dooropen1;
48	dooropen2;
49	dooropen3;
50	dooropen4;
51	breakhold
52	lkason
53	lowlamp
54	foglamp
55	highlamp
56	
57	
58	lead_one_getstatus
59	lead_two_getstatus
60	float lead_vertices_0_x;
61	float lead_vertices_0_y;
62	float lead_vertices_1_x;
63	float lead_vertices_1_y;
64	float lane_blindspot_probs_0;
65	float lane_blindspot_probs_1;
66	float lane_line_probs_1;
67	float lane_line_probs_2;  
68	float lane_line_probs_3;  
69	float lane_line_probs_4;  
70	track_vertices_cnt
71	track_vertices_xy
203	track_vertices_xy
204	lane_line_vertices_0_cnt
205	lane_line_vertices_0_xy
337	lane_line_vertices_0_xy
338	lane_line_vertices_1_cnt
339	lane_line_vertices_1_xy
471	lane_line_vertices_1_xy
472	lane_line_vertices_2_cnt
473	lane_line_vertices_2_xy
605	lane_line_vertices_2_xy
606	lane_line_vertices_3_cnt
607	lane_line_vertices_3_xy
739	lane_line_vertices_3_xy
	
	
	
750	tcar[0] x
751	tcar[0] y
752	tcar[0] state
753	tcar[1] x
754	tcar[1] y
755	tcar[1] state
	….
850

*/