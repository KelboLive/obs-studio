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
