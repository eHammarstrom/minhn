#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <jansson.h>

#define PRINTLN(fmt, ...) printf(fmt "\n", ##__VA_ARGS__);

#define FATAL(fmt, ...)                                                        \
	fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__);                    \
	exit(1);

#define VPRINTLN(fmt, ...)                                                     \
	if (verbose)                                                           \
		printf(fmt "\n", ##__VA_ARGS__);

#define ITER_ARGS(stmts)                                                       \
	for (int i = 0; i < argc; ++i) {                                       \
		stmts                                                          \
	}

#define HAS_ARG(var, str)                                                      \
	if (strncmp(argv[i], str, sizeof(str)) == 0)                           \
		var = true;

#define HTTP_BUFSIZ (1024 * 1024)

#define NUM_STORIES (5)

static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
	return fwrite(ptr, size, nmemb, (FILE *)data);
}

/* allocates bufsize if supplied buf is NULL */
int request_body(char *buf, size_t bufsiz, char *url)
{
	CURL *curl;
	CURLcode res;
	FILE *http_buf;

	if (!buf) {
		buf = malloc(bufsiz);
	}

	http_buf = fmemopen(buf, bufsiz, "w+");

	if (!http_buf) {
		return false;
	}

	curl = curl_easy_init();

	if (!curl) {
		goto req_err;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, http_buf);

	res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		goto req_err;
	}

	/* success */
	fclose(http_buf);
	return true;

req_err:
	fclose(http_buf);
	return false;
}

size_t prettify_stories(char **str, json_t **stories)
{
	size_t buf_len;
	FILE *f;

	f = open_memstream(str, &buf_len);

	if (!f) {
		FATAL("failed to open write stream");
	}

	for (int i = 0; i < NUM_STORIES; ++i) {
		json_t *story, *jurl, *jtitle;
		const char *url, *title;

		story = stories[i];
		jurl = json_object_get(story, "url");
		jtitle = json_object_get(story, "title");

		url = json_string_value(jurl);
		title = json_string_value(jtitle);

		fprintf(f, "%d: %s\n", i + 1, title);

		if (i + 1 == NUM_STORIES)
			fprintf(f, "\t(%s)", url);
		else
			fprintf(f, "\t(%s)\n\n", url);

		json_decref(jurl);
		json_decref(jtitle);
	}

	fclose(f);

	return buf_len;
}

int main(int argc, char *argv[])
{
	char *output;

	bool help = false;
	bool verbose = false;

	char stories_text[HTTP_BUFSIZ] = { 0 };
	char story_text[HTTP_BUFSIZ] = { 0 };
	char story_text_url[256] = { 0 };

	json_error_t jerror;
	json_t *stories_root;
	json_t *stories[NUM_STORIES] = { NULL };

	ITER_ARGS(
		HAS_ARG(help, "-h");
		HAS_ARG(help, "--help");
		HAS_ARG(verbose, "-v");
		HAS_ARG(verbose, "--verbose");
	);

	if (help) {
		PRINTLN("Usage: minhn [OPTION...]");
		PRINTLN("\t-v, --verbose");
		PRINTLN("\t-h, --help");
		return 0;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (!request_body(
		    stories_text, HTTP_BUFSIZ,
		    "https://hacker-news.firebaseio.com/v0/topstories.json")) {
		FATAL("could not retrieve top stories");
	}

	stories_root = json_loads(stories_text, 0, &jerror);

	if (!stories_root) {
		char *msg = jerror.text;
		FATAL("failed to parse top stories, %s:\n%s", msg,
		      stories_text);
	}

	if (!json_is_array(stories_root)) {
		FATAL("received unknown json blob");
	}

	for (int i = 0; i < NUM_STORIES; ++i) {
		json_t *element, *story_root;
		json_int_t id;

		element = json_array_get(stories_root, i);

		if (!json_is_integer(element)) {
			FATAL("received unknown json blob of ids");
		}

		id = json_integer_value(element);

		sprintf(story_text_url,
			"https://hacker-news.firebaseio.com/v0/item/%lld.json",
			id);

		if (!request_body(story_text, HTTP_BUFSIZ, story_text_url)) {
			FATAL("could not retrieve story");
		}

		story_root = json_loads(story_text, 0, &jerror);
		if (!json_is_object(story_root)) {
			FATAL("received unknown json story blob, %s:\n%s",
			      (char *)jerror.text, story_text);
		}

		stories[i] = story_root;
	}

	/* output length return unused */
	prettify_stories(&output, stories);

	PRINTLN("%s", output);

	/* free json blobs */
	json_decref(stories_root);

	for (int i = 0; i < NUM_STORIES; ++i) {
		json_decref(stories[i]);
	}

	return 0;
}
