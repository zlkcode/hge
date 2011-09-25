/*
** Haaf's Game Engine 1.8
** Copyright (C) 2003-2007, Relish Games
** hge.relishgames.com
**
** Core functions implementation: resources management
*/


#include "hge_impl.h"

#define NOCRYPT
//#define NOUNCRYPT
#include <unzip.h>


bool HGE_CALL HGE_Impl::Resource_AttachPack(hgeConstString filename, const char * password)
{
	hgeString szName;
	CResourceList *resItem=res;
	unzFile zip;

	szName=Resource_MakePath(filename);
	hge_strupr(szName);

	while(resItem)
	{
		if( ! hge_strcmp(szName,resItem->filename) ) return false;
		resItem=resItem->next;
	}
	
#if HGE_UNICODE
	char utf8_fn[_MAX_PATH*2];
	hgeWideToUtf8( szName, utf8_fn, sizeof(utf8_fn) );
	zip=unzOpen(utf8_fn);
#else
	zip=unzOpen(szName);
#endif
	if(!zip) return false;
	unzClose(zip);

	resItem=new CResourceList;
	hge_strcpy(resItem->filename, szName);
	if(password) strcpy(resItem->password, password);
	else resItem->password[0]=0;
	resItem->next=res;
	res=resItem;

	return true;
}

void HGE_CALL HGE_Impl::Resource_RemovePack(hgeConstString filename)
{
	hgeString szName;
	CResourceList *resItem=res, *resPrev=0;

	szName=Resource_MakePath(filename);
	hge_strupr(szName);

	while(resItem)
	{
		if( ! hge_strcmp(szName,resItem->filename) )
		{
			if(resPrev) resPrev->next=resItem->next;
			else res=resItem->next;
			delete resItem;
			break;
		}

		resPrev=resItem;
		resItem=resItem->next;
	}
}

void HGE_CALL HGE_Impl::Resource_RemoveAllPacks()
{
	CResourceList *resItem=res, *resNextItem;

	while(resItem)
	{
		resNextItem=resItem->next;
		delete resItem;
		resItem=resNextItem;
	}

	res=0;
}

void* HGE_CALL HGE_Impl::Resource_Load(hgeConstString filename, uint32_t *size)
{
	static hgeConstString res_err = TXT("Can't load resource: %s");

	CResourceList *resItem=res;
	hgeChar szName[_MAX_PATH];
	hgeChar szZipName[_MAX_PATH];
	unzFile zip;
	unz_file_info file_info;
	int done, i;
	void *ptr;
	HANDLE hF;

	if(filename[0]=='\\' || filename[0]=='/' || filename[1]==':') goto _fromfile; // skip absolute paths

	// Load from pack
 
	hge_strcpy(szName,filename);
	hge_strupr(szName);
	for(i=0; szName[i]; i++) { if(szName[i]=='/') szName[i]='\\'; }

	while(resItem)
	{
#if HGE_UNICODE
		char utf8_fn[_MAX_PATH*2];
		hgeWideToUtf8( resItem->filename, utf8_fn, sizeof(utf8_fn) );
		zip=unzOpen(utf8_fn);
#else
		zip=unzOpen(resItem->filename);
#endif

		done=unzGoToFirstFile(zip);
		while(done==UNZ_OK)
		{
#if HGE_UNICODE
			char szZipName_utf8[_MAX_PATH*2];
			hgeStrToUtf8( szZipName, szZipName_utf8, sizeof(szZipName_utf8) );
			unzGetCurrentFileInfo(zip, &file_info, szZipName_utf8, sizeof(szZipName), NULL, 0, NULL, 0);
#else
			unzGetCurrentFileInfo(zip, &file_info, szZipName, sizeof(szZipName), NULL, 0, NULL, 0);
#endif
			hge_strupr(szZipName);
			for(i=0; szZipName[i]; i++)	{ if(szZipName[i]=='/') szZipName[i]='\\'; }
			if( ! hge_strcmp(szName,szZipName) )
			{
// #if HGE_UNICODE
// 				char pass_utf8[64];
// 				hgeWideToUtf8( resItem->password, pass_utf8, sizeof(utf8_fn) );
// 				if(unzOpenCurrentFilePassword(zip, pass_utf8[0] ? pass_utf8 : 0) != UNZ_OK)
// #else
				if(unzOpenCurrentFilePassword(zip, resItem->password[0] ? resItem->password : 0) != UNZ_OK)
// #endif
				{
					unzClose(zip);
					hge_sprintf(szName, sizeof szName, res_err, filename);
					_PostError(szName);
					return 0;
				}

				// add extra 1 byte for trailing zero, incase we load and use string from resource
				ptr = malloc(file_info.uncompressed_size + 1);
				((char *)ptr)[file_info.uncompressed_size] = 0;

				if(!ptr)
				{
					unzCloseCurrentFile(zip);
					unzClose(zip);
					hge_sprintf(szName, sizeof szName, res_err, filename);
					_PostError(szName);
					return 0;
				}

				if(unzReadCurrentFile(zip, ptr, file_info.uncompressed_size) < 0)
				{
					unzCloseCurrentFile(zip);
					unzClose(zip);
					free(ptr);
					hge_sprintf(szName, sizeof szName, res_err, filename);
					_PostError(szName);
					return 0;
				}
				unzCloseCurrentFile(zip);
				unzClose(zip);
				if(size) *size=file_info.uncompressed_size;
				return ptr;
			}
			
			done=unzGoToNextFile(zip);
		}
		
		unzClose(zip);
		resItem=resItem->next;
	}

	// Load from file
_fromfile:

	hF = HGE_WINAPI_UNICODE_SUFFIX(CreateFile)(
				Resource_MakePath(filename),
				GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL
				);
	if(hF == INVALID_HANDLE_VALUE)
	{
		hge_sprintf(szName, sizeof szName, res_err, filename);
		_PostError(szName);
		return 0;
	}
	file_info.uncompressed_size = GetFileSize(hF, NULL);
	ptr = malloc(file_info.uncompressed_size);
	if(!ptr)
	{
		CloseHandle(hF);
		hge_sprintf(szName, sizeof szName, res_err, filename);
		_PostError(szName);
		return 0;
	}
	if(ReadFile(hF, ptr, file_info.uncompressed_size, &file_info.uncompressed_size, NULL ) == 0)
	{
		CloseHandle(hF);
		free(ptr);
		hge_sprintf(szName, sizeof szName, res_err, filename);
		_PostError(szName);
		return 0;
	}
	
	CloseHandle(hF);
	if(size) *size=file_info.uncompressed_size;
	return ptr;
}


void HGE_CALL HGE_Impl::Resource_Free(void *res)
{
	if(res) free(res);
}


hgeString HGE_CALL HGE_Impl::Resource_MakePath(hgeConstString filename)
{
	int i;

	if(!filename)
		hge_strcpy(szTmpFilename, szAppPath);
	else if(filename[0]=='\\' || filename[0]=='/' || filename[1]==':')
		hge_strcpy(szTmpFilename, filename);
	else
	{
		hge_strcpy(szTmpFilename, szAppPath);
		if(filename) hge_strcat(szTmpFilename, filename);
	}

	for(i=0; szTmpFilename[i]; i++) { if(szTmpFilename[i]=='/') szTmpFilename[i]='\\'; }
	return szTmpFilename;
}

hgeString HGE_CALL HGE_Impl::Resource_EnumFiles(hgeConstString wildcard)
{
	if(wildcard)
	{
		if(hSearch) { FindClose(hSearch); hSearch=0; }
		hSearch=HGE_WINAPI_UNICODE_SUFFIX(FindFirstFile)(
						Resource_MakePath(wildcard), &SearchData
						);
		if(hSearch==INVALID_HANDLE_VALUE) { hSearch=0; return 0; }

		if(!(SearchData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return SearchData.cFileName;
		else return Resource_EnumFiles();
	}
	else
	{
		if(!hSearch) return 0;
		for(;;)
		{
			if(! HGE_WINAPI_UNICODE_SUFFIX(FindNextFile)(hSearch, &SearchData))	{
				FindClose(hSearch);
				hSearch=0;
				return 0;
			}
			if(!(SearchData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				return SearchData.cFileName;
		}
	}
}

hgeString HGE_CALL HGE_Impl::Resource_EnumFolders(hgeConstString wildcard)
{
	if(wildcard)
	{
		if(hSearch) { FindClose(hSearch); hSearch=0; }
		hSearch = HGE_WINAPI_UNICODE_SUFFIX(FindFirstFile)(
							Resource_MakePath(wildcard), &SearchData
							);
		if(hSearch==INVALID_HANDLE_VALUE) { hSearch=0; return 0; }

		if((SearchData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
			&& hge_strcmp(SearchData.cFileName, TXT(".") )
			&& hge_strcmp(SearchData.cFileName, TXT("..") ))
		{
			return SearchData.cFileName;
		}
		else return Resource_EnumFolders();
	}
	else
	{
		if(!hSearch) return 0;
		for(;;)
		{
			if( ! HGE_WINAPI_UNICODE_SUFFIX(FindNextFile)( hSearch, &SearchData )) { 
				FindClose(hSearch);
				hSearch=0;
				return 0; 
			}
			if((SearchData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
				&& hge_strcmp(SearchData.cFileName, TXT(".") )
				&& hge_strcmp(SearchData.cFileName, TXT("..") ))
			{
				return SearchData.cFileName;
			}
		}
	}
}
