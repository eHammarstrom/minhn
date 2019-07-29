#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <curl/curl.h>
#include <jansson.h>

#define PRINTLN(fmt, ...) printf(fmt "\n", ##__VA_ARGS__);

#define FATAL(fmt, ...)                                                        \
	do {                                                                   \
		fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__);            \
		exit(1);                                                       \
	} while (0)

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

#define HAS_INT_ARG(var, min, max)                                             \
	do {                                                                   \
		unsigned long num = strtoul(argv[i], NULL, 10);                \
		if (errno != ERANGE) {                                         \
			if (num >= min && num <= max) {                        \
				var = num;                                     \
			} else if (num > 1) {                                  \
				FATAL("NUM_STORIES must be >= 1 and <= 500."); \
			}                                                      \
		}                                                              \
		errno = 0;                                                     \
	} while (0)

#define HTTP_BUFSIZ (10 * 1024)

int request_body(char *buf, size_t bufsiz, char *url)
{
	CURL *curl;
	CURLcode res;
	FILE *http_buf;

	http_buf = fmemopen(buf, bufsiz, "w+");

	if (!http_buf) {
		return false;
	}

	curl = curl_easy_init();

	if (!curl) {
		goto req_err;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
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

size_t prettify_stories(char **str, json_t **stories, unsigned int num_stories)
{
	size_t buf_len;
	FILE *f;

	f = open_memstream(str, &buf_len);

	if (!f) {
		FATAL("failed to open write stream");
	}

	for (int i = 0; i < num_stories; ++i) {
		json_t *story, *jurl, *jtitle;
		const char *url, *title;

		story = stories[i];
		jurl = json_object_get(story, "url");
		jtitle = json_object_get(story, "title");

		url = json_string_value(jurl);
		title = json_string_value(jtitle);

		fprintf(f, "%d: %s\n", i + 1, title);

		if (i + 1 == num_stories)
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

	unsigned long num_stories = 5;

	char text_buf[HTTP_BUFSIZ] = { 0 };
	char story_text_url[256] = { 0 };

	json_error_t jerror;
	json_auto_t *stories_root;
	json_t **stories;

	ITER_ARGS(
		HAS_ARG(help, "-h");
		HAS_ARG(help, "--help");
		HAS_ARG(verbose, "-v");
		HAS_ARG(verbose, "--verbose");
		HAS_INT_ARG(num_stories, 1, 500);
	);

	stories = calloc(num_stories, sizeof(json_t *));

	if (help) {
		PRINTLN("Usage: minhn [OPTION...] [num_stories]");
		PRINTLN("\t-v, --verbose");
		PRINTLN("\t-h, --help");
		return 0;
	}

	curl_global_init(CURL_GLOBAL_DEFAULT);

	if (!request_body(
		    text_buf, HTTP_BUFSIZ,
		    "https://hacker-news.firebaseio.com/v0/topstories.json")) {
		FATAL("could not retrieve top stories");
	}

	stories_root = json_loads(text_buf, 0, &jerror);

	if (!stories_root) {
		char *msg = jerror.text;
		FATAL("failed to parse top stories, %s:\n%s", msg, text_buf);
	}

	if (!json_is_array(stories_root)) {
		FATAL("received unknown json blob");
	}

	for (int i = 0; i < num_stories; ++i) {
		json_auto_t *element;
		json_t *story_root;
		json_int_t id;

		element = json_array_get(stories_root, i);

		if (!json_is_integer(element)) {
			FATAL("received unknown json blob of ids");
		}

		id = json_integer_value(element);

		sprintf(story_text_url,
			"https://hacker-news.firebaseio.com/v0/item/%lld.json",
			id);

		if (!request_body(text_buf, HTTP_BUFSIZ, story_text_url)) {
			FATAL("could not retrieve story");
		}

		story_root = json_loads(text_buf, 0, &jerror);
		if (!json_is_object(story_root)) {
			FATAL("received unknown json story blob, %s:\n%s",
			      (char *)jerror.text, text_buf);
		}

		stories[i] = story_root;
	}

	prettify_stories(&output, stories, num_stories);

	PRINTLN("%s", output);

	/* cleanup */

	for (int i = 0; i < num_stories; ++i)
		json_decref(stories[i]);

	free(stories);
	free(output);

	return 0;
}
