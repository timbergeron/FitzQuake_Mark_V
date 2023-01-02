


#ifndef __DOWNLOADS_H__
#define __DOWNLOADS_H__

#define DOWNLOADS_BASE "http://quake-1.com/mods"  // i.e.  quake-1.com/mods/
#define DOWNLOADS_LOCAL_BASE "_mods" // i.e. c:/quake/_mods


// Issues:  

// warp.warpstart.quoth	// warp folder, start map is warp, quoth is the hudstyle  (how would engine know the startmap?)
// qt_pre02.travail.id1	// how does know to install in travail folder?

// Returns a buffer; receiver is responsible for freeing it!
void Downloads_Init (void);
qboolean Download_URL_Designated_Folder (const char* basefile);
void Download_Remote_Zip_Install (const char* basefile);
void Download_Remote_Zip_Install_f (void);


#endif // __DOWNLOADS_H__
