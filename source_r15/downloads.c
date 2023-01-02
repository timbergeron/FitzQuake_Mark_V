/*

Downloads

*/

#include "quakedef.h"
#include <curl.h>
#pragma comment (lib, "delayimp.lib") // This is windows only so platform specific pragma = fine!
#pragma comment (lib, "libcurl.lib") // This is windows only so platform specific pragma = fine!


static int Web_Init( void );
static void Web_Cleanup( void );

// to get the size and mime type before the download we can use that
// http://curl.haxx.se/mail/lib-2002-05/0036.html

// Baker: NO ... use #define APPLICATION "Qrack"

static CURL *curl = NULL;
static char curl_err[1024];
static int ( *progress )(double)= NULL;

static size_t Write( void *ptr, size_t size, size_t nmemb, void *stream )
{
	FILE *f;
	//Con_Printf("name %s path %s\n", data->name, data->path);
	f = FS_fopen_write_create_path ( (char *)stream, "ab");

	if( f == NULL )
		return size*nmemb; // weird

	//fwrite(ptr,size,nmemb,(FILE *) stream);
	fwrite( ptr, size, nmemb, f );
	fclose( f );

	return size*nmemb;
}

static int Progress( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
	// callback
	if( progress != NULL )
	{
		if( progress( dlnow/dltotal ) != 0 )
			return 1; // we abort
	}

	//Con_Printf("Progress: %2.2f\n",dlnow*100.0/dltotal);
	return 0;
}

static void Web_Cleanup( void )
{
	if( curl != NULL )
	{
		/* always cleanup */
		curl_easy_cleanup( curl );
		curl = NULL;

		// reset callback
		progress = NULL;
	}
}

static int Web_Init( void )
{
	CURLcode code;
	char useragent[256];

	if( curl != NULL )
	{
		Web_Cleanup();
	}

	// reinit
	curl = curl_easy_init();
	// reset callback
	progress = NULL;

	// http://curl.haxx.se/libcurl/c/curl_easy_setopt.html
	/* init some options of curl */
	code = curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, curl_err );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set error buffer\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_NOPROGRESS, 0 );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set NoProgress\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1 );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set libcurl nosignal mode\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1 );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set FollowLocation\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_MAXREDIRS, 2 );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set Max Redirection\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, Write );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set writer callback function\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_PROGRESSFUNCTION, Progress );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set progress callback function\n" );
		return 0;
	}

	q_strlcpy (useragent, "MyAgent", sizeof(useragent));

	code = curl_easy_setopt( curl, CURLOPT_USERAGENT, useragent);

	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set UserAgent\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_FAILONERROR, 1 );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set fail on error\n" );
		return 0;
	}

	return 1;
}

int Web_Get( const char *url, const char *referer, const char *name, int resume, int max_downloading_time, int timeout, int ( *_progress )(double) )
{
	CURLcode code;
	FILE *f;
	unsigned int fsize;

	// init/reinit curl
	Web_Init();

	code = curl_easy_setopt( curl, CURLOPT_URL, url );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set url\n" );
		return 0;
	}

	if( referer )
	{
		code = curl_easy_setopt( curl, CURLOPT_REFERER, referer );
		if( code != CURLE_OK )
		{
			Con_Printf( "Failed to set Referer\n" );
			return 0;
		}
	}

	// connection timeout
	code = curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT, timeout );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set libcurl connection timeout\n" );
		return 0;
	}

	code = curl_easy_setopt( curl, CURLOPT_TIMEOUT, max_downloading_time );
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set libcurl global timeout\n" );
		return 0;
	}

	if( resume == 1 )
	{
		// test if file exist
		if( ( f = FS_fopen_read( name, "r" ) ) == NULL )
		{
			// file does not exist
			goto new_file;
		}
		// the file exist
		// get the size
		fsize = fseek( f, 0, SEEK_END );
		fclose( f );

		code = curl_easy_setopt( curl, CURLOPT_RESUME_FROM, fsize );
		if( code != CURLE_OK )
		{
			Con_Printf( "Failed to set file resume from length\n" );
			return 0;
		}

		// file exist all good
	}
	else
	{
		// we will append to the file so if it already exist we will have twice the data
		// so delete the file if it exist
		remove( name );
	}

new_file:

	code = curl_easy_setopt( curl, CURLOPT_FILE, name );
	//code=curl_easy_setopt(curl, CURLOPT_FILE, &f);
	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to set writer data\n" );
		return 0;
	}

	// set callback
	progress = _progress;
	code = curl_easy_perform( curl );

	if (progress != NULL)
		Con_Printf( "Downloading %s from %s\n", name, url );

	if( code != CURLE_OK )
	{
		Con_Printf( "Failed to download %s from %s\n", name, url );
		Con_Printf( "Error: %s\n", curl_err );

		Web_Cleanup();
		return 0;
	}

	Web_Cleanup();
	return 1;
}



	static char *my_user_agent = "libcurl-agent/1.0";	

	struct MemoryStruct
	{
		char *memory;
		size_t size;
	};

	static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
	{
		size_t realsize = size * nmemb;
		struct MemoryStruct *mem = (struct MemoryStruct *)userp;

		mem->memory = realloc(mem->memory, mem->size + realsize + 1);
		if (mem->memory == NULL)
		{
			// out of memory!
			printf("not enough memory (realloc returned NULL)\n");
			exit(EXIT_FAILURE);
		}

		memcpy(&(mem->memory[mem->size]), contents, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;

		return realsize;
	}

	unsigned char* URL_Download_Alloc (const char* my_internet_url, size_t* bytesRead)
	{
		CURL *curl_handle;
		struct MemoryStruct chunk = {0};
		unsigned char* ret_buffer = NULL;

		chunk.memory = malloc(1);  // will be grown as needed by the realloc above
		
		curl_global_init(CURL_GLOBAL_ALL);
			curl_handle = curl_easy_init();			
			curl_easy_setopt(curl_handle, CURLOPT_URL, my_internet_url); // http://google.com
			curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); // send all data to this function
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);  //we pass our 'chunk' struct to the callback function
			curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, my_user_agent);
			
			curl_easy_perform(curl_handle); // EXECUTE
			curl_easy_cleanup(curl_handle); // Cleanup
		curl_global_cleanup();

		if (chunk.size)
		{
			// Success.  Copy to a buffer.  +1 so if string has null termination for sure.
			ret_buffer = calloc (1, chunk.size + 1);
			memcpy (ret_buffer, chunk.memory, chunk.size);
		}

		free (chunk.memory);

		*bytesRead = chunk.size;
		return ret_buffer;
	}

	size_t URL_Download_HEAD (const char* my_internet_url) //, size_t* bytesRead /* other stuff? */)
	{
		CURL *curl_handle;
		struct MemoryStruct chunk = {0};  // chunk.size = 0;
		int curl_code;
		int http_code = 0;
		size_t size_of_file = 0;

		chunk.memory = malloc(1);  // will be grown as needed by the realloc above */		

		curl_global_init(CURL_GLOBAL_ALL);
			curl_handle = curl_easy_init();	// init curl session
			curl_easy_setopt(curl_handle, CURLOPT_URL, my_internet_url); // Specify url		
			curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1);  // HEAD REQUEST
			curl_easy_setopt(curl_handle, CURLOPT_FILETIME); // Get file time.  You have to ask!
#if 1			
			curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); // Send all data to this function
			curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);   // we pass our 'chunk' struct to the callback function
#endif			
			curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, my_user_agent); // Make up a user agent

			curl_code = curl_easy_perform(curl_handle); // EXECUTE

			
			// Determine response code	
			curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &http_code);
			
			if (http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK)
			{
				size_of_file = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD);
			}
			Con_Printf ("(%i) %s: Curl response code is %i\n", curl_code, my_internet_url, http_code);
			
			curl_easy_cleanup(curl_handle); // Cleanup


		curl_global_cleanup(); // we're done with libcurl

		free (chunk.memory);

		if (http_code == 200)
			return size_of_file;
		else return 0;
	}

	void Download_Remote_Zip_Install (const char* basefile)
	{
		
		if ( Download_URL_Designated_Folder (basefile) )
		{
			Con_Printf ("1) File %s downloaded we think\n", basefile);
			// Now unzip
			Con_Printf ("2) Changing dir to %s\n", va("%s/%s", com_basedir, DOWNLOADS_LOCAL_BASE) );
			Sys_chdir ( va("%s/%s", com_basedir, DOWNLOADS_LOCAL_BASE) );
			Sys_UnZip_File (basefile);
			Sys_chdir ( com_basedir ); // Change it back, a DLL might need to be in the current dir

		} else Con_Printf ("File % failed to download\n", basefile);
		
		
	}

	void Download_Remote_Zip_Install_f (void)
	{
		// NEXT:  Determine if the file exists.

		if (Cmd_Argc() == 2)
		{
			const char* basefile = Cmd_Argv (1);
			Download_Remote_Zip_Install (basefile);
		} else Con_Printf ("No filename specified\n");


	}


	qboolean Download_URL_Designated_Folder (const char* basefile)
	{
			
			#pragma message ("Check the name make sure no dots or crap")
			char url_to_get[MAX_OSPATH];
			char filename[MAX_OSPATH];
			size_t bytesread;
			byte* data = NULL;
			
			
			q_snprintf (url_to_get, sizeof(url_to_get), "%s/%s", DOWNLOADS_BASE,  basefile);
			q_snprintf (filename, sizeof(filename), "%s/%s/%s", com_basedir, DOWNLOADS_LOCAL_BASE, basefile);
			
			// Stage 1, verify resource
			{
				size_t bytes_to_download = URL_Download_HEAD (url_to_get);
				if (!bytes_to_download)
				{
					Con_Printf ("FAILED:  The file %s does not exist\n", url_to_get);
					return false;


				}
				Con_Printf ("File exists to download of size %u\n", (int)bytes_to_download);
			}

			Con_Printf ("Download request: %s --> %s\n", url_to_get, filename);

			data = URL_Download_Alloc(url_to_get, &bytesread);
			if (data)
			{
				Con_Printf ("Download read %d bytes\n", bytesread);
				File_Memory_To_File (filename, data, bytesread);
				Sys_OpenFolder_HighlightFile (filename);
	
				free(data);
			} else return false; // Download failed


			return true;
	}

	// Mission!  Write a file to disk!!!!
	void Downloads_Download_f (void)
	{
		// NEXT:  Determine if the file exists.

		if (Cmd_Argc() == 2)
		{
			const char* basefile = Cmd_Argv (1);
		} else Con_Printf ("No filename specified\n");


	}

	void Downloads_Init (void)
	{

		Cmd_AddCommand ("download", Downloads_Download_f);
		Cmd_AddCommand ("downloadzip", Download_Remote_Zip_Install_f);
		// %s/%s/ the trailing path is required
		COM_CreatePath (va("%s/%s/", com_basedir, DOWNLOADS_LOCAL_BASE)); // Makes c:\quake\_mods folder

	}
