#include "ICalendar.h"

/* MODULE INTERNAL STATE */

/** The maximum length of a line, in octets, before it's folded (RFC 5545, 3.1). */
#define MAXIMUM_LINE_LENGTH 75

/** A growing text, to build the values of the properties. */
typedef struct {
	char * characters;
	size_t length;
	size_t capacity;
} Text;

/** An event of the calendar: the entries of a block, from "first" to "last". */
typedef struct {
	const Entry * first;
	const Entry * last;
} Event;

/* PRIVATE FUNCTIONS */

static void _append(Text * text, const char * format, ...);
static void _appendEscaped(Text * text, const char * value);
static int _characterLength(const unsigned char lead);
static const char * _describeTopic(const Symbol * topic);
static void _writeEvent(FILE * file, const Event * event, const char * stamp);
static void _writeProperty(FILE * file, const char * name, const char * value);

static void _append(Text * text, const char * format, ...) {
	va_list arguments;
	va_start(arguments, format);
	const int length = vsnprintf(NULL, 0, format, arguments);
	va_end(arguments);
	if (text->capacity <= text->length + length) {
		text->capacity = 2 * (text->length + length + 1);
		text->characters = realloc(text->characters, text->capacity);
	}
	va_start(arguments, format);
	vsnprintf(text->characters + text->length, length + 1, format, arguments);
	va_end(arguments);
	text->length += length;
}

/**
 * Appends a value of the TEXT type, which escapes the backslashes, the
 * commas, the semicolons and the line breaks (RFC 5545, 3.3.11).
 */
static void _appendEscaped(Text * text, const char * value) {
	for (const char * character = value; *character != '\0'; ++character) {
		switch (*character) {
			case '\\': _append(text, "\\\\"); break;
			case ',': _append(text, "\\,"); break;
			case ';': _append(text, "\\;"); break;
			case '\n': _append(text, "\\n"); break;
			default: _append(text, "%c", *character);
		}
	}
}

/** The amount of octets of an UTF-8 character, from its first octet. */
static int _characterLength(const unsigned char lead) {
	if (lead < 0x80) {
		return 1;
	}
	if ((lead & 0xE0) == 0xC0) {
		return 2;
	}
	if ((lead & 0xF0) == 0xE0) {
		return 3;
	}
	return 4;
}

/** The "description" of a topic, or NULL without one. */
static const char * _describeTopic(const Symbol * topic) {
	for (const MemberList * node = topic->topic->members; node != NULL; node = node->next) {
		if (node->member->type == ATTRIBUTE_MEMBER && strcmp(node->member->attribute->name, "description") == 0) {
			return node->member->attribute->values->expression->factor->constant->string;
		}
	}
	return NULL;
}

/**
 * Writes the event of a block: its topic and goal as the summary, and its
 * method and its sessions as the description. The event lasts until its
 * last session ends.
 */
static void _writeEvent(FILE * file, const Event * event, const char * stamp) {
	const Entry * first = event->first;
	const Symbol * topic = first->topic;
	const int date = toDate(first->day);
	int sessions = 0;
	int study = 0;
	int end = first->start;
	for (const Entry * entry = first; entry <= event->last; ++entry) {
		if (entry->isSession) {
			++sessions;
			study += entry->duration;
			end = entry->start + entry->duration;
		}
	}
	char value[128];
	fprintf(file, "BEGIN:VEVENT\r\n");
	snprintf(value, sizeof(value), "%08dT%02d%02d-%s@stujfy", date, first->start / 60, first->start % 60, topic->name);
	_writeProperty(file, "UID", value);
	_writeProperty(file, "DTSTAMP", stamp);
	snprintf(value, sizeof(value), "%08dT%02d%02d00", date, first->start / 60, first->start % 60);
	_writeProperty(file, "DTSTART", value);
	snprintf(value, sizeof(value), "%08dT%02d%02d00", date, end / 60, end % 60);
	_writeProperty(file, "DTEND", value);
	Text text = { 0 };
	_appendEscaped(&text, topic->name);
	_append(&text, " (");
	_appendEscaped(&text, topic->owner->name);
	_append(&text, ")");
	_writeProperty(file, "SUMMARY", text.characters);
	text.length = 0;
	char studyText[16];
	formatDuration(study, studyText);
	if (first->isFixed) {
		_append(&text, "Sesión fija de %s.", studyText);
	}
	else if (first->method == NULL) {
		_append(&text, "Bloque de estudio de %s.", studyText);
	}
	else {
		_appendEscaped(&text, first->method);
		if (sessions == 1) {
			_append(&text, ": 1 sesión\\, %s de estudio.", studyText);
		}
		else {
			_append(&text, ": %d sesiones\\, %s de estudio.", sessions, studyText);
		}
	}
	const char * description = _describeTopic(topic);
	if (description != NULL) {
		_append(&text, "\\n");
		_appendEscaped(&text, description);
	}
	if (first != event->last) {
		_append(&text, "\\n");
		for (const Entry * entry = first; entry <= event->last; ++entry) {
			const int stop = entry->start + entry->duration;
			_append(&text, "\\n%02d:%02d a %02d:%02d: %s", entry->start / 60, entry->start % 60,
				stop / 60, stop % 60, entry->isSession ? "estudio" : "pausa");
		}
	}
	_writeProperty(file, "DESCRIPTION", text.characters);
	text.length = 0;
	_appendEscaped(&text, topic->owner->name);
	_writeProperty(file, "CATEGORIES", text.characters);
	free(text.characters);
	fprintf(file, "END:VEVENT\r\n");
}

/**
 * Writes a property ("NAME:value"), ending with CRLF. A line longer than 75
 * octets is folded: it continues in the next line, after a space, and
 * without cutting an UTF-8 character (RFC 5545, 3.1).
 */
static void _writeProperty(FILE * file, const char * name, const char * value) {
	int column = fprintf(file, "%s:", name);
	for (const char * character = value; *character != '\0';) {
		const int length = _characterLength((unsigned char) *character);
		if (MAXIMUM_LINE_LENGTH < column + length) {
			fprintf(file, "\r\n ");
			column = 1;
		}
		fwrite(character, 1, length, file);
		column += length;
		character += length;
	}
	fprintf(file, "\r\n");
}

/* PUBLIC FUNCTIONS */

bool generateICalendar(const Schedule * schedule, const char * path) {
	FILE * file = fopen(path, "wb");
	if (file == NULL) {
		return false;
	}
	char stamp[17];
	const time_t now = time(NULL);
	strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", gmtime(&now));
	fprintf(file, "BEGIN:VCALENDAR\r\n");
	_writeProperty(file, "VERSION", "2.0");
	_writeProperty(file, "PRODID", "-//Stujfy//Compilador de Stujfy//ES");
	_writeProperty(file, "CALSCALE", "GREGORIAN");
	_writeProperty(file, "METHOD", "PUBLISH");
	_writeProperty(file, "X-WR-CALNAME", "Plan de estudio");
	for (int k = 0; k < schedule->count; ++k) {
		const int last = lastOfBlock(schedule, k);
		const Event event = { .first = &schedule->entries[k], .last = &schedule->entries[last] };
		_writeEvent(file, &event, stamp);
		k = last;
	}
	fprintf(file, "END:VCALENDAR\r\n");
	return fclose(file) == 0;
}
