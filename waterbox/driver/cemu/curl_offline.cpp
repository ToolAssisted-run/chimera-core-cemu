// libcurl for a machine with no network: Cemu's network code (the
// console's own curl library, its online services) is compiled against
// curl's headers and linked to this. Every transfer fails with "could not
// connect", natively and in the sandbox alike, so both are the same machine
// and nothing leaves the computer.
#define CURL_DISABLE_TYPECHECK
#include <curl/curl.h>

#include <cstdarg>
#include <cstdlib>
#include <cstring>

namespace
{
	struct OfflineHandle
	{
		int kind;
	};
}

extern "C"
{
	CURLcode curl_global_init(long flags) { return CURLE_OK; }
	CURLcode curl_global_init_mem(long flags, curl_malloc_callback m, curl_free_callback f, curl_realloc_callback r, curl_strdup_callback s, curl_calloc_callback c) { return CURLE_OK; }
	void curl_global_cleanup(void) {}

	CURL* curl_easy_init(void) { return (CURL*)new OfflineHandle{1}; }
	CURLcode curl_easy_setopt(CURL* curl, CURLoption option, ...) { return CURLE_OK; }
	CURLcode curl_easy_perform(CURL* curl) { return CURLE_COULDNT_CONNECT; }
	void curl_easy_cleanup(CURL* curl) { delete (OfflineHandle*)curl; }
	void curl_easy_reset(CURL* curl) {}
	CURLcode curl_easy_pause(CURL* handle, int bitmask) { return CURLE_OK; }
	CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...)
	{
		va_list ap;
		va_start(ap, info);
		void* out = va_arg(ap, void*);
		va_end(ap);
		if (!out)
			return CURLE_BAD_FUNCTION_ARGUMENT;
		switch (info & CURLINFO_TYPEMASK)
		{
		case CURLINFO_STRING: *(char**)out = nullptr; break;
		case CURLINFO_LONG: *(long*)out = 0; break;
		case CURLINFO_DOUBLE: *(double*)out = 0.0; break;
		case CURLINFO_OFF_T: *(curl_off_t*)out = 0; break;
		default: *(void**)out = nullptr; break;
		}
		return CURLE_OK;
	}
	const char* curl_easy_strerror(CURLcode code) { return code == CURLE_OK ? "No error" : "No network in this core"; }

	struct curl_slist* curl_slist_append(struct curl_slist* list, const char* data)
	{
		auto* item = (struct curl_slist*)malloc(sizeof(struct curl_slist));
		item->data = strdup(data ? data : "");
		item->next = nullptr;
		if (!list)
			return item;
		struct curl_slist* last = list;
		while (last->next)
			last = last->next;
		last->next = item;
		return list;
	}
	void curl_slist_free_all(struct curl_slist* list)
	{
		while (list)
		{
			struct curl_slist* next = list->next;
			free(list->data);
			free(list);
			list = next;
		}
	}

	CURLFORMcode curl_formadd(struct curl_httppost** httppost, struct curl_httppost** last_post, ...) { return CURL_FORMADD_OK; }
	void curl_formfree(struct curl_httppost* form) {}

	CURLM* curl_multi_init(void) { return (CURLM*)new OfflineHandle{2}; }
	CURLMcode curl_multi_add_handle(CURLM* multi_handle, CURL* curl_handle) { return CURLM_OK; }
	CURLMcode curl_multi_remove_handle(CURLM* multi_handle, CURL* curl_handle) { return CURLM_OK; }
	CURLMcode curl_multi_fdset(CURLM* multi_handle, fd_set* read_fd_set, fd_set* write_fd_set, fd_set* exc_fd_set, int* max_fd)
	{
		if (max_fd)
			*max_fd = -1;
		return CURLM_OK;
	}
	CURLMcode curl_multi_perform(CURLM* multi_handle, int* running_handles)
	{
		if (running_handles)
			*running_handles = 0;
		return CURLM_OK;
	}
	CURLMcode curl_multi_cleanup(CURLM* multi_handle)
	{
		delete (OfflineHandle*)multi_handle;
		return CURLM_OK;
	}
	CURLMsg* curl_multi_info_read(CURLM* multi_handle, int* msgs_in_queue)
	{
		if (msgs_in_queue)
			*msgs_in_queue = 0;
		return nullptr;
	}
	CURLMcode curl_multi_setopt(CURLM* multi_handle, CURLMoption option, ...) { return CURLM_OK; }
	CURLMcode curl_multi_timeout(CURLM* multi_handle, long* milliseconds)
	{
		if (milliseconds)
			*milliseconds = -1;
		return CURLM_OK;
	}

	CURLSH* curl_share_init(void) { return (CURLSH*)new OfflineHandle{3}; }
	CURLSHcode curl_share_setopt(CURLSH* share, CURLSHoption option, ...) { return CURLSHE_OK; }
	CURLSHcode curl_share_cleanup(CURLSH* share)
	{
		delete (OfflineHandle*)share;
		return CURLSHE_OK;
	}
}
