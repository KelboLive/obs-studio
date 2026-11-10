/* The production registry adapter over a thread-local stand-in core, so the
 * Rust get_source_info can be driven per test thread. */
#include "libobs/build/libobs.pch.h"

#if defined(_MSC_VER)
static __declspec(thread) struct obs_core source_info_lookup_core;
#else
static _Thread_local struct obs_core source_info_lookup_core;
#endif

static const struct obs_core *source_info_lookup_current_core(void)
{
	return &source_info_lookup_core;
}

#define obs_core_texture_activity_core source_info_lookup_current_core
#include "source_info_lookup_fields.inc"
#undef obs_core_texture_activity_core

/* Make `types` (borrowed, not copied) this thread's registered source types. */
void oracle_source_info_lookup_use(struct obs_source_info *types, size_t num);
void oracle_source_info_lookup_use(struct obs_source_info *types, size_t num)
{
	source_info_lookup_core.source_types.array = num ? types : NULL;
	source_info_lookup_core.source_types.num = num;
	source_info_lookup_core.source_types.capacity = num;
}

/* The original lookup, renamed, over the same thread-local core. */
struct obs_source_info *oracle_get_source_info(const char *id);
#define obs_core_texture_activity_core source_info_lookup_current_core
#define get_source_info oracle_get_source_info
#include "libobs/obs-source-info-lookup.c.h"
#undef get_source_info
#undef obs_core_texture_activity_core

#define SOURCE_INFO_LOOKUP_MAX 32

/* Register `ids` (none NULL) on this thread, then look `id` up with the
 * production get_source_info (`lookup`) and the original C. Writes each
 * found index (or -1) and returns whether the registry is unchanged and both
 * found the same entry address. `id` may be NULL only when `num` is 0: the
 * original crashes on it otherwise. */
typedef struct obs_source_info *(*source_info_lookup_fn)(const char *);
bool oracle_source_info_lookup_check(const char *const *ids, size_t num, const char *id, source_info_lookup_fn lookup,
				     ptrdiff_t *rust_index, ptrdiff_t *c_index);
bool oracle_source_info_lookup_check(const char *const *ids, size_t num, const char *id, source_info_lookup_fn lookup,
				     ptrdiff_t *rust_index, ptrdiff_t *c_index)
{
	struct obs_source_info types[SOURCE_INFO_LOOKUP_MAX] = {0};
	struct obs_source_info before[SOURCE_INFO_LOOKUP_MAX];
	if (num > SOURCE_INFO_LOOKUP_MAX || (id == NULL && num != 0))
		return false;
	for (size_t i = 0; i < num; i++) {
		if (!ids[i])
			return false;
		types[i].id = ids[i];
		types[i].version = (uint32_t)i;
	}
	memcpy(before, types, sizeof(types));
	oracle_source_info_lookup_use(types, num);

	struct obs_source_info *rust = lookup(id);
	struct obs_source_info *c = oracle_get_source_info(id);
	*rust_index = rust ? rust - types : -1;
	*c_index = c ? c - types : -1;
	bool same = rust == c && memcmp(before, types, sizeof(types)) == 0;
	oracle_source_info_lookup_use(NULL, 0);
	return same;
}

/* The intentional differences, production side only: with a registered NULL
 * ID first, a NULL asked ID and the empty ID find nothing, and "a" finds the
 * second entry. The original is not called: it would crash in strcmp. */
bool oracle_source_info_lookup_null_ids(source_info_lookup_fn lookup);
bool oracle_source_info_lookup_null_ids(source_info_lookup_fn lookup)
{
	struct obs_source_info types[2] = {{.id = NULL}, {.id = "a"}};
	oracle_source_info_lookup_use(types, 2);
	bool ok = lookup(NULL) == NULL && lookup("") == NULL && lookup("a") == &types[1];
	oracle_source_info_lookup_use(NULL, 0);
	return ok;
}
