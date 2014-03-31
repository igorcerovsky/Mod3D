#define WM_STARTCOMPUTETHREAD				(WM_USER + 1)
#define WM_COMPUTETHREADFINISHED			(WM_USER + 2)
#define WM_COMPUTETHREADCANCELED			(WM_USER + 3)

#define WM_VIEW_3D_PROPERTIES				(WM_USER + 10)	// send when 3D View properties changed
#define WM_BODY_PROPERTIES					(WM_USER + 11)	// send when body properties changed
#define WM_VIEW_3D_SAVE_SETTINGS			(WM_USER + 12)	// send when view should save current settings
#define WM_VIEW_3D_LOAD_SETTINGS			(WM_USER + 13)	// send when view should load current settings

#define WM_UPDATE_OBJMNG					(WM_USER + 14)	// send when object manager needs update
