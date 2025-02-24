#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <thread>

#include "cJSON.h"
#include "config.h"

void print_config(config_t *config)
{
	// Sender tags
	for (uint16_t i = 0; i < UINT16_MAX; ++i)
	{
		if (config->sender_tags[i] != NULL)
		{
			knxnet::address_t a = {.value = i};
			printf("%2u.%2u.%3u ", a.pa.area, a.pa.line, a.pa.member);
			bool first = true;
			tags_t *entry = config->sender_tags[i];
			while (entry != NULL)
			{
				if (first)
				{
					first = false;
				}
				else
				{
					printf("          ");
				}
				printf("+ [");
				bool first_tag = true;
				for (size_t i = 0; i < entry->tags_len; ++i)
				{
					if (first_tag)
					{
						first_tag = false;
					}
					else
					{
						printf(", ");
					}
					printf("%s", entry->tags[i]);
				}
				printf("]\n");
				entry = entry->next;
			}
		}
	}
	printf("\n");

	// Group addresses
	for (uint16_t i = 0; i < UINT16_MAX; ++i)
	{
		if (config->gas[i] != NULL)
		{
			knxnet::address_t a = {.value = i};
			ga_t *entry = config->gas[i];
			tags_t *ga_tags_entry = config->ga_tags[i];
			bool first = true;

			if (ga_tags_entry != nullptr && ga_tags_entry->read_on_startup)
			{
				printf(">");
			}
			else
			{
				printf(" ");
			}

			printf("%2u/%2u/%3u ", a.ga.area, a.ga.line, a.ga.member);

			while (entry != NULL)
			{
				if (first)
				{
					first = false;
				}
				else
				{
					printf("           ");
				}
				printf("-> %s (DPT %u%s) ", entry->series, entry->dpt, (entry->convert_to_int ? " conv to int" : (entry->convert_to_float ? " conv to float" : "")));

				bool first_tag = true;
				printf("[");
				for (size_t t = 0; t < entry->tags_len; ++t)
				{
					if (first_tag)
					{
						first_tag = false;
					}
					else
					{
						printf(", ");
					}
					printf("%s", entry->tags[t]);
				}
				printf("]\n");

				while (ga_tags_entry != nullptr){
					first_tag = true;
					if (ga_tags_entry->tags_len > 0)
					{
						printf("           +  [");
					}
					for (size_t t = 0; t < ga_tags_entry->tags_len; ++t)
					{
						if (first_tag)
						{
							first_tag = false;
						}
						else
						{
							printf(", ");
						}
						printf("%s", ga_tags_entry->tags[t]);
					}
					if (ga_tags_entry->tags_len > 0)
					{
						printf("]\n");
					}
					ga_tags_entry = ga_tags_entry->next;
				}
				entry = entry->next;
			}
		}
	}
}

long safe_strtol(char const *str, char **endptr, int base)
{
	errno = 0;
	long val = strtol(str, endptr, base);
	if (errno != 0)
	{
		printf("error parsing value in GA\n");
		exit(EXIT_FAILURE);
	}
	return val;
}

knxnet::address_arr_t *parse_pa(const char *pa)
{
	// printf("parsing %s\n", pa);
	auto addrs = parse_addr(pa, ".", 15, 15);
	// address_t *cur = addrs;
	// while (cur->value != 0)
	// {
	// 	printf("%u.%u.%u\n", cur->pa.area, cur->pa.line, cur->pa.member);
	// 	cur++;
	// }

	// printf("----------\n");
	return addrs;
}

knxnet::address_arr_t *parse_ga(const char *ga)
{
	// printf("parsing %s\n", ga);
	auto addrs = parse_addr(ga, "/", 31, 7);
	// address_t *cur = addrs;
	// while (cur->value != 0)
	// {
	// 	printf("%u/%u/%u\n", cur->ga.area, cur->ga.line, cur->ga.member);
	// 	cur++;
	// }

	// printf("----------\n");
	return addrs;
}

knxnet::address_arr_t *parse_addr(const char *addr_s, const char *sep, uint8_t area_max, uint8_t line_max)
{
	char *string = strdup(addr_s);
	char *tofree = string;
	uint8_t astart = 0, aend = 0;
	uint8_t lstart = 0, lend = 0;
	uint8_t mstart = 0, mend = 0;

	char *area_s = strsep(&string, sep);
	if (area_s == NULL)
	{
		printf("error parsing addr (area)\n");
		exit(EXIT_FAILURE);
	}
	char *line_s = strsep(&string, sep);
	if (line_s == NULL)
	{
		printf("error parsing addr (line)\n");
		exit(EXIT_FAILURE);
	}
	char *member_s = strsep(&string, sep);
	if (member_s == NULL)
	{
		printf("error parsing addr (member)\n");
		exit(EXIT_FAILURE);
	}

	// Area
	if (area_s[0] == '[')
	{
		// Range
		char *start_s = strsep(&area_s, "-");
		if (start_s == NULL)
		{
			printf("error parsing range addr (start)\n");
			exit(EXIT_FAILURE);
		}
		start_s++;
		char *end_s = strsep(&area_s, "-");
		if (end_s == NULL)
		{
			printf("error parsing range addr (start)\n");
			exit(EXIT_FAILURE);
		}
		end_s[strlen(end_s)-1] = '\0';
		//printf("%s - %s\n", start_s, end_s);
		astart = safe_strtol(start_s, NULL, 10);
		aend = safe_strtol(end_s, NULL, 10);
	}
	else if (area_s[0] == '*')
	{
		// Wildard
		astart = 0;
		aend = area_max;
	}
	else
	{
		astart = safe_strtol(area_s, NULL, 10);
		aend = astart;
	}

	// Line
	if (line_s[0] == '[')
	{
		// Range
		char *start_s = strsep(&line_s, "-");
		if (start_s == NULL)
		{
			printf("error parsing range addr (start)\n");
			exit(EXIT_FAILURE);
		}
		start_s++;
		char *end_s = strsep(&line_s, "-");
		if (end_s == NULL)
		{
			printf("error parsing range addr (start)\n");
			exit(EXIT_FAILURE);
		}
		end_s[strlen(end_s)-1] = '\0';
		//printf("%s - %s\n", start_s, end_s);
		lstart = safe_strtol(start_s, NULL, 10);
		lend = safe_strtol(end_s, NULL, 10);
	}
	else if (line_s[0] == '*')
	{
		// Wildard
		lstart = 0;
		lend = line_max;
	}
	else
	{
		lstart = safe_strtol(line_s, NULL, 10);
		lend = lstart;
	}

	// Member
	if (member_s[0] == '[')
	{
		// Range
		char *start_s = strsep(&member_s, "-");
		if (start_s == NULL)
		{
			printf("error parsing range addr (start)\n");
			exit(EXIT_FAILURE);
		}
		start_s++;
		char *end_s = strsep(&member_s, "-");
		if (end_s == NULL)
		{
			printf("error parsing range addr (start)\n");
			exit(EXIT_FAILURE);
		}
		end_s[strlen(end_s)-1] = '\0';
		//printf("%s - %s\n", start_s, end_s);
		mstart = safe_strtol(start_s, NULL, 10);
		mend = safe_strtol(end_s, NULL, 10);
	}
	else if (member_s[0] == '*')
	{
		// Wildard
		mstart = 0;
		mend = 255;
	}
	else
	{
		mstart = safe_strtol(member_s, NULL, 10);
		mend = mstart;
	}

	free(tofree);

	knxnet::address_arr_t *addrs = new knxnet::address_arr_t;
	knxnet::address_t cur;
	for (uint16_t a = astart; a <= aend; ++a)
		for (uint16_t l = lstart; l <= lend; ++l)
			for (uint16_t m = mstart; m <= mend; ++m)
			{
				if (a == 0 && l == 0 && m == 0)
				{
					continue;
				}

				// Hacky
				if (line_max == 7)
				{
					cur.ga.area = a;
					cur.ga.line = l;
					cur.ga.member = m;
				}
				else
				{
					cur.pa.area = a;
					cur.pa.line = l;
					cur.pa.member = m;
				}

				addrs->add(cur);
			}

	return addrs;
}

int parse_config(config_t *config, void (*periodic_read_fkt)(knx_timer_t *timer))
{
	int status = 0;

	// Open config file
	FILE *f = fopen(config->file, "rb");
	if (f == NULL)
	{
		std::cerr << "Could not find config file " << config->file << "!" << std::endl;
		status = -1;
		return status;
	}
	fseek(f, 0, SEEK_END);
	uint64_t fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	// Read file
	char *json_str = (char *)malloc(fsize + 1);
	fread(json_str, fsize, 1, f);
	fclose(f);
	json_str[fsize] = '\0';

	std::string error_ptr;
	// And parse

	int i = 0;
	cJSON *json = cJSON_Parse(json_str);
	cJSON *obj1 = nullptr, *obj2 = nullptr, *obj3 = nullptr, *obj4 = nullptr;
	knxnet::address_t *cur;
	knxnet::address_arr_t *addrs1 = nullptr;

	if (json == NULL)
	{
		error_ptr = (char *)cJSON_GetErrorPtr();
		goto error;
	}

	obj1 = cJSON_GetObjectItemCaseSensitive(json, "interface");
	if (cJSON_IsString(obj1) && (obj1->valuestring != NULL))
	{
		config->interface = strdup(obj1->valuestring);
	}
	else
	{
		error_ptr = "No interface given in config!";
		goto error;
	}

	obj1 = cJSON_GetObjectItemCaseSensitive(json, "host");
	if (cJSON_IsString(obj1) && (obj1->valuestring != NULL))
	{
		config->host = strdup(obj1->valuestring);
	}
	else
	{
		error_ptr = "No host given in config!";
		goto error;
	}

	obj1 = cJSON_GetObjectItemCaseSensitive(json, "database");
	if (cJSON_IsString(obj1) && (obj1->valuestring != NULL))
	{
		config->database = strdup(obj1->valuestring);
	}
	else
	{
		error_ptr = "No database given in config!";
		goto error;
	}

	obj1 = cJSON_GetObjectItemCaseSensitive(json, "user");
	if (obj1 && cJSON_IsString(obj1) && (obj1->valuestring != NULL))
	{
		config->user = strdup(obj1->valuestring);
	}

	obj1 = cJSON_GetObjectItemCaseSensitive(json, "password");
	if (obj1 && cJSON_IsString(obj1) && (obj1->valuestring != NULL))
	{
		config->password = strdup(obj1->valuestring);
	}

	obj1 = cJSON_GetObjectItemCaseSensitive(json, "token");
	if (obj1 && cJSON_IsString(obj1) && (obj1->valuestring != NULL))
	{
		config->token = strdup(obj1->valuestring);
	}

	// Sender tags
	obj1 = cJSON_GetObjectItemCaseSensitive(json, "sender_tags");
	// sender_tags is optional
	if (obj1)
	{
		if (!cJSON_IsObject(obj1))
		{
			error_ptr = "Expected object, got something else for 'sender_tags'";
			goto error;
		}

		obj2 = NULL;
		cJSON_ArrayForEach(obj2, obj1)
		{
			tags_t sender_tag = {};

			if (!cJSON_IsArray(obj2))
			{
				error_ptr = "Expected array as value, got something else for entry in 'sender_tags'";
				goto error;
			}

			sender_tag.tags_len = cJSON_GetArraySize(obj2);
			//sender_tag.tags = (char **)calloc(sender_tag.tags_len, sizeof(char *));
			sender_tag.tags = new char*[sender_tag.tags_len];

			i = 0;

			obj3 = NULL;
			cJSON_ArrayForEach(obj3, obj2)
			{
				if (!cJSON_IsString(obj3))
				{
					error_ptr = "Expected string for tag entry in 'sender_tags'";
					goto error;
				}
				if (obj3->valuestring == NULL)
				{
					error_ptr = "Got empty string instead of a tag entry in 'sender_tags'";
					goto error;
				}

				sender_tag.tags[i] = strdup(obj3->valuestring);

				++i;
			}

			addrs1 = parse_pa(obj2->string);
			cur = addrs1->addrs;
			while (cur->value != 0)
			{
				tags_t *__sender_tag;
				//__sender_tag = (tags_t *)calloc(1, sizeof(tags_t));
				__sender_tag = new tags_t;
				memcpy(__sender_tag, &sender_tag, sizeof(tags_t));

				if (config->sender_tags[cur->value] == NULL)
				{
					config->sender_tags[cur->value] = __sender_tag;
				}
				else
				{
					tags_t *entry;
					entry = config->sender_tags[cur->value];
					while (entry->next != NULL)
						entry = entry->next;
					entry->next = __sender_tag;
				}
				cur++;
			}
			//printf("free: %p\n", addrs);
			free(addrs1);
		}
	}

	// Periodic reading
	obj1 = cJSON_GetObjectItemCaseSensitive(json, "periodic_read");
	if (obj1)
	{
		if (!cJSON_IsObject(obj1))
		{
			error_ptr = "Expected object, got something else for 'periodic_read'";
			goto error;
		}

		// Iterate over object
		obj2 = NULL;
		knx_timer_t *last = nullptr;
		cJSON_ArrayForEach(obj2, obj1)
		{
			if (!cJSON_IsArray(obj2))
			{
				error_ptr = "Expected array as value, got something else for entry in 'periodic_read'";
				goto error;
			}
			knx_timer_t *t = new knx_timer_t;
			t->next = nullptr;
			t->interval = strtoul(obj2->string, nullptr, 10);
			if (errno == ERANGE)
			{
				error_ptr = "Invalid interval!";
				goto error;
			}
			// Iterate over the arrays
			obj3 = NULL;
			cJSON_ArrayForEach(obj3, obj2)
			{
				if (!cJSON_IsString(obj3))
				{
					error_ptr = "Expected string for ga entry in 'periodic_read'";
					goto error;
				}
				if (obj3->valuestring == NULL)
				{
					error_ptr = "Got empty string instead of a ga entry in 'periodic_read'";
					goto error;
				}
				if (t->addrs == nullptr)
				{
					t->addrs = parse_ga(obj3->valuestring);
				}
				else
				{
					// Merge current addresses with the new ones
					addrs1 = parse_ga(obj3->valuestring);
					t->addrs->add(addrs1);
					delete addrs1;
				}
			}

			t->thread = new std::thread(periodic_read_fkt, t);

			if (last == nullptr)
			{
				// is first entry
				config->timers = t;
			}
			else
			{
				last->next = t;
			}
			last = t;
		}
	}

	// Group address tags
	obj1 = cJSON_GetObjectItemCaseSensitive(json, "ga_tags");
	// ga_tags is optional
	if (obj1)
	{
		if (!cJSON_IsObject(obj1))
		{
			error_ptr = "Expected object, got something else for 'ga_tags'";
			goto error;
		}

		obj2 = NULL;
		cJSON_ArrayForEach(obj2, obj1)
		{
			tags_t ga_tag = {};

			if (!cJSON_IsArray(obj2))
			{
				error_ptr = "Expected array as value, got something else for entry in 'sender_tags'";
				goto error;
			}

			ga_tag.tags_len = cJSON_GetArraySize(obj2);
			//ga_tag.tags = (char **)calloc(_ga_tag.tags_len, sizeof(char *));
			ga_tag.tags = new char*[ga_tag.tags_len];

			i = 0;

			obj3 = NULL;
			cJSON_ArrayForEach(obj3, obj2)
			{
				if (!cJSON_IsString(obj3))
				{
					error_ptr = "Expected string for tag entry in 'sender_tags'";
					goto error;
				}
				if (obj3->valuestring == NULL)
				{
					error_ptr = "Got empty string instead of a tag entry in 'sender_tags'";
					goto error;
				}

				ga_tag.tags[i] = strdup(obj3->valuestring);

				++i;
			}

			addrs1 = parse_ga(obj2->string);
			cur = addrs1->addrs;

			while (cur->value != 0)
			{
				tags_t *__ga_tag;
				//__ga_tag = (tags_t *)calloc(1, sizeof(tags_t));
				__ga_tag = new tags_t;
				memcpy(__ga_tag, &ga_tag, sizeof(tags_t));

				if (config->ga_tags[cur->value] == NULL)
				{
					config->ga_tags[cur->value] = __ga_tag;
				}
				else
				{
					tags_t *entry;
					entry = config->ga_tags[cur->value];
					while (entry->next != NULL)
						entry = entry->next;
					entry->next = __ga_tag;
				}
				cur++;
			}
			//printf("free: %p\n", addrs);
			free(addrs1);
		}
	}

	// Startup reads
	obj1 = cJSON_GetObjectItemCaseSensitive(json, "read_on_startup");
	// read_on_startup is optional
	if (obj1)
	{
		if (!cJSON_IsArray(obj1))
		{
			error_ptr = "Expected array, got something else for 'ga_tags'";
			goto error;
		}

		obj2 = NULL;
		cJSON_ArrayForEach(obj2, obj1)
		{
			if (!cJSON_IsString(obj2))
			{
				error_ptr = "Expected string for GA entry in 'read_on_startup'";
				goto error;
			}
			if (obj2->valuestring == NULL)
			{
				error_ptr = "Got empty string instead of a GA entry in 'read_on_startup'";
				goto error;
			}

			addrs1 = parse_ga(obj2->valuestring);
			cur = addrs1->addrs;

			while (cur->value != 0)
			{
				if (config->ga_tags[cur->value] == NULL)
				{
					//config->ga_tags[cur->value] = (tags_t *)calloc(1, sizeof(tags_t));
					config->ga_tags[cur->value] = new tags_t;
					config->ga_tags[cur->value]->read_on_startup = true;
				}
				else
				{
					tags_t *entry;
					entry = config->ga_tags[cur->value];
					while (entry != NULL)
					{
						entry->read_on_startup = true;
						entry = entry->next;
					}
				}
				cur++;
			}
			free(addrs1);
		}
	}

	// Group addresses
	obj1 = cJSON_GetObjectItemCaseSensitive(json, "gas");
	if (!cJSON_IsArray(obj1))
	{
		error_ptr = "Expected array, got something else for 'gas'";
		goto error;
	}

	obj2 = NULL;

	cJSON_ArrayForEach(obj2, obj1)
	{
		if (!cJSON_IsObject(obj2))
		{
			error_ptr = "Expected array of ojects, got something that is not object in 'gas'";
			goto error;
		}

		ga_t _ga = {};

		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "series");
		if (!cJSON_IsString(obj3))
		{
			error_ptr = "'series' is not a string!";
			goto error;
		}
		if (obj3->valuestring == NULL)
		{
			error_ptr = "'series' must not be empty!";
			goto error;
		}
		//printf("Series: %s\n", series->valuestring);
		_ga.series = strdup(obj3->valuestring);

		// Read out DPT
		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "dpt");
		if (!cJSON_IsNumber(obj3))
		{
			error_ptr = "'dpt' is not a number!";
			goto error;
		}
		_ga.dpt = (uint16_t)obj3->valueint;

		// Read out SubDPT
		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "subdpt");
		if (obj3)
		{
			if (!cJSON_IsNumber(obj3))
			{
				error_ptr = "'subdpt' is not a number!";
				goto error;
			}

			_ga.subdpt = (uint16_t)obj3->valueint;
		}
		else
		{
			_ga.subdpt = 0;
		}

		// If DPT is 1, find out if we should convert to int
		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "convert_to_int");
		i = 0;
		if (obj3)
		{
			if (!cJSON_IsBool(obj3))
			{
				error_ptr = "'convert_to_int' is not a bool!";
				goto error;
			}

			i = obj3->type == cJSON_True ? 1 : 0;
		}
		_ga.convert_to_int = i;
		
		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "convert_to_float");
		i = 0;
		if (obj3)
		{
			if (!cJSON_IsBool(obj3))
			{
				error_ptr = "'convert_to_float' is not a bool!";
				goto error;
			}

			i = obj3->type == cJSON_True ? 1 : 0;
		}
		_ga.convert_to_float = i;
		if (_ga.convert_to_int && _ga.convert_to_float)
		{
			error_ptr = "cannot convert to int and float at the same time!";
			goto error;
		}

		// Check if we should only log to stdout
		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "log_only");
		i = 0;
		if (obj3)
		{
			if (!cJSON_IsBool(obj3))
			{
				error_ptr = "'log_only' is not a bool!";
				goto error;
			}

			i = obj3->type == cJSON_True ? 1 : 0;
		}
		_ga.log_only = i;

		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "ignored_senders");
		if (obj3)
		{
			if (!cJSON_IsArray(obj3))
			{
				error_ptr = "'ignored_senders' is not an array!";
				goto error;
			}
			_ga.ignored_senders_len = cJSON_GetArraySize(obj3);
			//_ga.ignored_senders = (knxnet::address_t *)calloc(_ga.ignored_senders_len, sizeof(knxnet::address_t));
			_ga.ignored_senders = new knxnet::address_t[_ga.ignored_senders_len];

			i = 0;
			obj4 = NULL;
			cJSON_ArrayForEach(obj4, obj3)
			{
				if (!cJSON_IsString(obj4))
				{
					error_ptr = "Expected array of strings, got something that is not a string in 'ignored_senders'";
					goto error;
				}
				if (obj4->valuestring == NULL)
				{
					error_ptr = "Got empty string instead of a physical address in 'ignored_senders'";
					goto error;
				}
				uint32_t area, line, member;
				sscanf(obj4->valuestring, "%u.%u.%u", &area, &line, &member);
				_ga.ignored_senders[i].pa.area = area;
				_ga.ignored_senders[i].pa.line = line;
				_ga.ignored_senders[i].pa.member = member;
				++i;
			}
		}

		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "tags");
		if (obj3)
		{
			if (!cJSON_IsArray(obj3))
			{
				error_ptr = "'tags' is not an array!";
				goto error;
			}
			_ga.tags_len = cJSON_GetArraySize(obj3);
			//_ga.tags = (char **)calloc(_ga.tags_len, sizeof(char *));
			_ga.tags = new char*[_ga.tags_len];

			i = 0;
			obj4 = NULL;
			cJSON_ArrayForEach(obj4, obj3)
			{
				if (!cJSON_IsString(obj4))
				{
					error_ptr = "Expected array of string, got something that is not a string in 'tags'";
					goto error;
				}
				if (obj4->valuestring == NULL)
				{
					error_ptr = "Got empty string instead of a key=value pair in 'tags'";
					goto error;
				}
				_ga.tags[i] = strdup(obj4->valuestring);
				++i;
			}
		}
		else
		{
			_ga.tags_len = 0;
			_ga.tags = NULL;
		}

		obj3 = cJSON_GetObjectItemCaseSensitive(obj2, "ga");
		if (!cJSON_IsString(obj3))
		{
			error_ptr = "'ga' is not a string!";
			goto error;
		}
		if (obj3->valuestring == NULL)
		{
			error_ptr = "'ga' must not be empty!";
			goto error;
		}
		//printf("GA: %s", ga->valuestring);

		addrs1 = parse_ga(obj3->valuestring);
		cur = addrs1->addrs;

		while (cur->value != 0)
		{
			ga_t *entry;
			entry = config->gas[cur->value];

			ga_t *__ga;
			//__ga = (ga_t *)calloc(1, sizeof(ga_t));
			__ga = new ga_t;
			memcpy(__ga, &_ga, sizeof(ga_t));

			if (entry == NULL)
			{
				config->gas[cur->value] = __ga;
			}
			else
			{
				while (entry->next != NULL)
				{
					entry = entry->next;
				}

				entry->next = __ga;
			}

			cur++;
		}
		//printf("free: %p\n", addrs);
		free(addrs1);
	}

	goto end;

error:
	std::cerr << "JSON error: " <<  error_ptr << std::endl;
	status = -1;
end:
	cJSON_Delete(json);
	free(json_str);
	return status;
}
