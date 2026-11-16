/*
CLUSTER_PARTITIONS.C

symbols in this file:
00180C10 0080:
	_reference_list_remove (0000)
00180C90 00c0:
	_reference_list_copy (0000)
00180D50 00d0:
	_cluster_partition_new (0000)
00180E20 0030:
	_cluster_partition_make_valid (0000)
00180E50 0030:
	_cluster_partition_make_invalid (0000)
00180E80 0030:
	_cluster_partition_delete (0000)
00180EB0 0030:
	_cluster_partition_get_next_datum (0000)
00180EE0 0040:
	_cluster_partition_get_first_cluster (0000)
00180F20 0030:
	_cluster_partition_get_next_cluster (0000)
00180F50 0050:
	_cluster_partition_copy (0000)
00180FA0 0050:
	_cluster_partition_get_first_reference (0000)
00180FF0 0200:
	_cluster_partition_reconnect (0000)
001811F0 00b0:
	_cluster_partition_disconnect (0000)
001812A0 0080:
	_cluster_partition_get_first_datum (0000)
002A0A94 003a:
	??_C@_0DK@NMKBPPFN@attempt?5to?5remove?5invalid?5elemen@ (0000)
002A0AD0 001d:
	??_C@_0BN@NFEHOLIC@?4?4?2objects?2reference_lists?4h?$AA@ (0000)
002A0AF0 002d:
	??_C@_0CN@HMHOKAKO@result?9?$DOmaximum_count?$DN?$DNsource?9?$DOm@ (0000)
002A0B20 001b:
	??_C@_0BL@LCCDAOLG@result?9?$DOsize?$DN?$DNsource?9?$DOsize?$AA@ (0000)
002A0B3C 002f:
	??_C@_0CP@NPCAPMLK@couldn?8t?5allocate?5?$CFs?5cluster?5par@ (0000)
002A0B6C 000b:
	??_C@_0L@LMJLHDGO@?$CFs?5cluster?$AA@ (0000)
002A0B78 000b:
	??_C@_0L@FHNEJCED@cluster?5?$CFs?$AA@ (0000)
002A0B84 0013:
	??_C@_0BD@HGMPGNNE@cluster?5references?$AA@ (0000)
002A0B98 004d:
	??_C@_0EN@JKAMLONC@cluster_index?$DO?$DN0?5?$CG?$CG?5cluster_inde@ (0000)
002A0BE8 002f:
	??_C@_0CP@DHFJNEOJ@c?3?2halo?2SOURCE?2structures?2cluste@ (0000)
002A0C18 0028:
	??_C@_0CI@IEBPGENP@an?5object?5or?5light?5spanned?5?$CFd?5cl@ (0000)
002A0C40 001f:
	??_C@_0BP@ELGEMBFB@?$CKfirst_cluster_reference?$DN?$DNNONE?$AA@ (0000)
002A0C60 0018:
	??_C@_0BI@INKNBGDF@first_cluster_reference?$AA@ (0000)
002A0C78 000a:
	??_C@_09IKAEIPAD@partition?$AA@ (0000)
*/

/* ---------- headers */

#include "cseries/cseries.h"
#include "cseries/errors.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "objects/reference_lists.h"
#include "physics/collision_bsp.h"
#include "physics/collision_bsp_definitions.h"
#include "saved games/game_state.h"
#include "scenario/scenario.h"
#include "cluster_partitions.h"
#include "structure_bsp_definitions.h"
#include "structures/structures.h"

/* ---------- constants */

/* ---------- macros */

/* ---------- structures */

/* ---------- prototypes */

void reference_list_remove(
	struct data_array *array,
	long *first_reference_index,
	long datum_index);
void reference_list_copy(
	struct data_array *result,
	struct data_array *source);

static long *cluster_partition_get_first_reference(
	struct cluster_partition *partition,
	short cluster_index);
static long *cluster_partition_get_first_reference_for_bsp(
	struct cluster_partition *partition,
	short bsp_index,
	short cluster_index);
static void cluster_partition_port_forget(
	struct cluster_partition *partition);
static long cluster_partition_port_add(
	struct data_array *array,
	long *first_reference_index,
	long datum_index,
	word pad_tag,
	long *previous_references);
static void cluster_partition_port_remove(
	struct cluster_partition *partition,
	long *first_reference_index,
	long datum_index,
	long found_reference_index);

/* ---------- globals */

/* port: the partitions (cluster_partition_new), whose references' places
cluster_partitions_port_forget forgets */
enum
{
	MAXIMUM_PORT_CLUSTER_PARTITIONS = 8,
};
static struct cluster_partition *cluster_partitions_port[MAXIMUM_PORT_CLUSTER_PARTITIONS];
static short cluster_partitions_port_count;
/* port: per-BSP cluster reference lists for BSPs 1..N-1 so objects and lights in
multiple resident BSPs do not collide on cluster indices 0..N-1; resynchronized
from data_reference_data whenever cluster_partition_port_forget runs */
struct cluster_partition_multi_bsp_state
{
	struct cluster_partition *partition;
	unsigned long synced_modification_count;
	long bsp_cluster_first_data_references[MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO][MAXIMUM_CLUSTERS_PER_STRUCTURE];
};
static struct cluster_partition_multi_bsp_state cluster_partitions_multi_bsp[MAXIMUM_PORT_CLUSTER_PARTITIONS];
static struct cluster_partition_multi_bsp_state *cluster_partition_multi_bsp_get(
	struct cluster_partition *partition);
/* port: the list of a cluster index that fits no cluster's
(cluster_partition_get_first_reference) */
static long cluster_partition_port_no_cluster_first_reference;
/* port: the full reference lists of this map (reference_list_port_full),
with how many times each was full again */
enum
{
	MAXIMUM_PORT_FULL_REFERENCE_LISTS = 8,
};
static struct
{
	struct data_array *array;
	unsigned long suppressed_count;
} reference_lists_port_full[MAXIMUM_PORT_FULL_REFERENCE_LISTS];

/* ---------- public code */

void reference_list_remove(
	struct data_array *array,
	long *first_reference_index,
	long datum_index)
{
	long *reference_index = first_reference_index;
	struct data_reference *reference;

	while (*reference_index != NONE)
	{
		reference = (struct data_reference *)datum_get(array, *reference_index);
		if (reference->datum_index == datum_index)
		{
			datum_delete(array, *reference_index);
			*reference_index = reference->next_reference_index;

			return;
		}

		reference_index = &reference->next_reference_index;
	}

	match_vassert(
		"..\\objects\\reference_lists.h",
		0x6d,
		FALSE,
		csprintf(temporary, "attempt to remove invalid element %ld from reference list", datum_index));

	return;
}

/* The initialization and cursor-advance order preserve January's coalescing
 * of the source cursor into EBX and the loop index into ESI. */
void reference_list_copy(
	struct data_array *result,
	struct data_array *source)
{
	short absolute_index;
	struct data_reference *source_reference;
	struct data_reference *result_reference;

	match_assert("..\\objects\\reference_lists.h", 0x88, result->size==source->size);
	match_assert("..\\objects\\reference_lists.h", 0x89, result->maximum_count==source->maximum_count);
	absolute_index = 0;
	result_reference = result->data;
	source_reference = source->data;
	while (absolute_index < result->maximum_count)
	{
		if (source_reference->identifier)
		{
			*result_reference = *source_reference;
		}
		else if (result_reference->identifier)
		{
			datum_delete(result, absolute_index);
		}

		absolute_index++;
		source_reference++;
		result_reference++;
	}

	return;
}

void cluster_partition_new(
	struct cluster_partition *partition,
	char const *name)
{
	char cluster_name[256];

	partition->cluster_first_data_references = game_state_malloc(
		name,
		"cluster references",
		MAXIMUM_CLUSTERS_PER_STRUCTURE * sizeof(*partition->cluster_first_data_references));

	sprintf(cluster_name, "cluster %s", name);
	/* the native builds' longer reference lists (halo_port_capacity.h): an
	object or light that cannot be referenced drops out of its clusters */
	partition->data_reference_data = reference_list_new(cluster_name, HALO_PORT_MAXIMUM_CLUSTER_REFERENCES);

	sprintf(cluster_name, "%s cluster", name);
	partition->cluster_reference_data = reference_list_new(cluster_name, HALO_PORT_MAXIMUM_CLUSTER_REFERENCES);

	if (!partition->cluster_first_data_references ||
		!partition->cluster_reference_data ||
		!partition->data_reference_data)
	{
		error(_error_immediate, "couldn't allocate %s cluster partition globals", name);
	}

	/* port: where its references are (cluster_partition_port_remove); none
	known yet */
	partition->port_previous_references = debug_malloc(HALO_PORT_MAXIMUM_CLUSTER_REFERENCES * sizeof(long), FALSE,
		__FILE__, __LINE__);
	partition->port_cluster_data_references = debug_malloc(HALO_PORT_MAXIMUM_CLUSTER_REFERENCES * sizeof(long), FALSE,
		__FILE__, __LINE__);
	cluster_partition_port_forget(partition);
	if (cluster_partitions_port_count < MAXIMUM_PORT_CLUSTER_PARTITIONS)
		cluster_partitions_port[cluster_partitions_port_count++] = partition;

	return;
}

void cluster_partition_make_valid(
	struct cluster_partition *partition)
{
	csmemset(
		partition->cluster_first_data_references,
		NONE,
		MAXIMUM_CLUSTERS_PER_STRUCTURE * sizeof(*partition->cluster_first_data_references));
	data_make_valid(partition->cluster_reference_data);
	data_make_valid(partition->data_reference_data);
	cluster_partition_port_forget(partition);
	/* port: the new map's lists warn again when full */
	reference_list_port_report(partition->cluster_reference_data);
	reference_list_port_report(partition->data_reference_data);

	return;
}

void cluster_partition_make_invalid(
	struct cluster_partition *partition)
{
	/* port: how many times the map's lists were full past the first */
	reference_list_port_report(partition->cluster_reference_data);
	reference_list_port_report(partition->data_reference_data);

	if (partition->cluster_reference_data->valid)
		data_make_invalid(partition->cluster_reference_data);

	if (partition->data_reference_data->valid)
		data_make_invalid(partition->data_reference_data);

	return;
}

void cluster_partition_delete(
	struct cluster_partition *partition)
{
	if (partition->cluster_first_data_references)
		partition->cluster_first_data_references = NULL;

	if (partition->cluster_reference_data)
		partition->cluster_reference_data = NULL;

	if (partition->data_reference_data)
		partition->data_reference_data = NULL;

	return;
}

void cluster_partition_copy(
	struct cluster_partition *result,
	struct cluster_partition const *source)
{
	/* port: no more clusters than the partitions hold (a map's bsp may say
	it has more) */
	csmemcpy(
		result->cluster_first_data_references,
		source->cluster_first_data_references,
		PIN(global_structure_bsp_get()->clusters.count, 0, MAXIMUM_CLUSTERS_PER_STRUCTURE) *
			sizeof(*result->cluster_first_data_references));
	reference_list_copy(
		result->cluster_reference_data,
		source->cluster_reference_data);
	reference_list_copy(
		result->data_reference_data,
		source->data_reference_data);
	cluster_partition_port_forget(result);

	return;
}

void cluster_partitions_port_forget(
	void)
{
	short index;

	for (index = 0; index < cluster_partitions_port_count; index++)
		cluster_partition_port_forget(cluster_partitions_port[index]);
}

/* port: A full reference list said so each time a reference did not fit:
on a map whose lights fill the light partition's lists, every light joining
its clusters each tick, thousands of lines a second in debug.txt (gigabytes
a day on a game left running). It says so the first time a map; the times
after are counted, and told when the map's lists are let go or made anew
(reference_list_port_report). What fits and what does not is as before. */
void reference_list_port_full(
	struct data_array *array)
{
	short index;
	short free_index = NONE;

	for (index = 0; index < MAXIMUM_PORT_FULL_REFERENCE_LISTS; index++)
	{
		if (reference_lists_port_full[index].array == array)
		{
			reference_lists_port_full[index].suppressed_count++;
			return;
		}
		if (!reference_lists_port_full[index].array && free_index == NONE)
			free_index = index;
	}
	if (free_index != NONE)
	{
		reference_lists_port_full[free_index].array = array;
		reference_lists_port_full[free_index].suppressed_count = 0;
	}
	error(_error_silent, "WARNING: maximum %ss per map (%d) exceeded.%s", array->name, array->maximum_count,
		free_index != NONE ? " (once a map: the rest are counted)" : "");
}

void reference_list_port_report(
	struct data_array *array)
{
	short index;

	for (index = 0; index < MAXIMUM_PORT_FULL_REFERENCE_LISTS; index++)
	{
		if (array && reference_lists_port_full[index].array == array)
		{
			if (reference_lists_port_full[index].suppressed_count)
			{
				error(_error_silent, "WARNING: maximum %ss per map (%d) exceeded %lu more times.", array->name,
					array->maximum_count, reference_lists_port_full[index].suppressed_count);
			}
			reference_lists_port_full[index].array = NULL;
			reference_lists_port_full[index].suppressed_count = 0;
		}
	}
}

long cluster_partition_get_next_datum(
	struct cluster_partition const *partition,
	long *reference_index)
{
	return reference_list_get_next_datum_index(partition->data_reference_data, reference_index);
}

long cluster_partition_get_first_cluster(
	struct cluster_partition const *partition,
	long *reference_index,
	long first_cluster_reference)
{
	*reference_index = first_cluster_reference;

	return cluster_partition_get_next_cluster(partition, reference_index);
}

long cluster_partition_get_next_cluster(
	struct cluster_partition const *partition,
	long *reference_index)
{
	/* port: in multi-BSP mode, filter cluster references by the active BSP stored in reference->pad */
	if (scenario_multi_bsp_active())
	{
		short const active_bsp = scenario_get_active_bsp_index();

		while (*reference_index != NONE)
		{
			struct data_reference *reference = (struct data_reference *)datum_get(
				partition->cluster_reference_data,
				*reference_index);
			short const ref_bsp = (short)reference->pad;
			long const cluster_index = reference->datum_index;

			*reference_index = reference->next_reference_index;
			if (ref_bsp == active_bsp)
			{
				return cluster_index;
			}
		}

		return NONE;
	}

	return reference_list_get_next_datum_index(partition->cluster_reference_data, reference_index);
}

void cluster_partition_reconnect(
	struct cluster_partition *partition,
	long datum_index,
	long *first_cluster_reference,
	real_point3d const *position,
	float radius,
	struct location const *location)
{
	short cluster_indices[64];
	short cluster_count;
	short cluster_index_index;
	short primary_bsp;
	struct cluster_partition_multi_bsp_state *multi_bsp_state;

	match_assert("c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0x6f, partition);
	match_assert("c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0x70, first_cluster_reference);
	match_assert("c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0x71, *first_cluster_reference==NONE);
	match_assert("c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0x72, position);
	match_assert("c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0x73, location);

	primary_bsp = scenario_get_active_bsp_index();
	multi_bsp_state = cluster_partition_multi_bsp_get(partition);

	cluster_count = structure_clusters_in_sphere(
		location->cluster_index,
		position,
		radius,
		NUMBEROF(cluster_indices),
		cluster_indices);

	if (cluster_count > 64)
	{
		error(_error_silent, "an object or light spanned %d clusters.", cluster_count);
		cluster_count = NUMBEROF(cluster_indices);
	}

	for (cluster_index_index = 0; cluster_index_index < cluster_count; cluster_index_index++)
	{
		short const cluster_index = cluster_indices[cluster_index_index];
		short const bsp_index = primary_bsp;
		word const data_pad = (cluster_index >= 0 && cluster_index < MAXIMUM_CLUSTERS_PER_STRUCTURE &&
			bsp_index >= 0 && bsp_index < MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO) ?
			(word)(((bsp_index & 0x1F) << 9) | (cluster_index & 0x1FF)) : 0xFFFF;
		/* port: reference_list_add's, noting where the references are */
		long cluster_reference_index = cluster_partition_port_add(
			partition->cluster_reference_data,
			first_cluster_reference,
			cluster_index,
			(word)bsp_index,
			NULL);
		long data_reference_index = cluster_partition_port_add(
			partition->data_reference_data,
			cluster_partition_get_first_reference_for_bsp(partition, bsp_index, cluster_index),
			datum_index,
			data_pad,
			partition->port_previous_references);

		partition->port_modification_count++;
		if (multi_bsp_state)
			multi_bsp_state->synced_modification_count = partition->port_modification_count;
		if (cluster_reference_index != NONE && partition->port_cluster_data_references)
		{
			partition->port_cluster_data_references[DATUM_INDEX_TO_ABSOLUTE_INDEX(cluster_reference_index)] =
				data_reference_index;
		}
	}

	/* port: Multi-BSP: also connect objects and lights located in BSP transition/overlap
	   zones into every other loaded BSP whose geometry contains or intersects the
	   bounding sphere, so doors, controls, scenery, weapons, units, projectiles,
	   and dynamic lights in overlap corridors remain visible from either BSP. */
	if (scenario_multi_bsp_active())
	{
		short other_bsp;
		boolean switched_bsp = FALSE;

		for (other_bsp = 0; other_bsp < MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO; other_bsp++)
		{
			short other_cluster_index = NONE;
			struct location other_location;
			struct structure_bsp const *other_structure_bsp;

			if (other_bsp == primary_bsp || !scenario_bsp_index_is_loaded(other_bsp))
			{
				continue;
			}

			other_structure_bsp = scenario_structure_bsp_get_by_index(other_bsp);
			if (!other_structure_bsp)
			{
				continue;
			}

			if (position->x + radius < other_structure_bsp->world_bounds.x0 ||
				position->x - radius > other_structure_bsp->world_bounds.x1 ||
				position->y + radius < other_structure_bsp->world_bounds.y0 ||
				position->y - radius > other_structure_bsp->world_bounds.y1 ||
				position->z + radius < other_structure_bsp->world_bounds.z0 ||
				position->z - radius > other_structure_bsp->world_bounds.z1)
			{
				continue;
			}

			if (scenario_test_point_in_bsp(other_bsp, position, &other_location) &&
				other_location.cluster_index >= 0 &&
				other_location.cluster_index < other_structure_bsp->clusters.count)
			{
				other_cluster_index = other_location.cluster_index;
			}
			else if (radius > 0.f)
			{
				static real const probe_dirs[6][3] =
				{
					{  1.f,  0.f,  0.f },
					{ -1.f,  0.f,  0.f },
					{  0.f,  1.f,  0.f },
					{  0.f, -1.f,  0.f },
					{  0.f,  0.f,  1.f },
					{  0.f,  0.f, -1.f }
				};
				real const probe_radius = MIN(radius, 0.5f);
				short probe_i;

				for (probe_i = 0; probe_i < 6; probe_i++)
				{
					real_point3d probe_pt;

					probe_pt.x = position->x + probe_dirs[probe_i][0] * probe_radius;
					probe_pt.y = position->y + probe_dirs[probe_i][1] * probe_radius;
					probe_pt.z = position->z + probe_dirs[probe_i][2] * probe_radius;

					if (scenario_test_point_in_bsp(other_bsp, &probe_pt, &other_location) &&
						other_location.cluster_index >= 0 &&
						other_location.cluster_index < other_structure_bsp->clusters.count)
					{
						other_cluster_index = other_location.cluster_index;
						break;
					}
				}
			}

			if (other_cluster_index != NONE)
			{
				short other_cluster_indices[64];
				short other_cluster_count;
				short other_i;

				scenario_set_active_bsp_context(other_bsp);
				switched_bsp = TRUE;

				other_cluster_count = structure_clusters_in_sphere(
					other_cluster_index,
					position,
					radius,
					NUMBEROF(other_cluster_indices),
					other_cluster_indices);

				if (other_cluster_count > 64)
				{
					other_cluster_count = NUMBEROF(other_cluster_indices);
				}

				for (other_i = 0; other_i < other_cluster_count; other_i++)
				{
					short const c_idx = other_cluster_indices[other_i];
					word const data_pad = (c_idx >= 0 && c_idx < MAXIMUM_CLUSTERS_PER_STRUCTURE) ?
						(word)(((other_bsp & 0x1F) << 9) | (c_idx & 0x1FF)) : 0xFFFF;
					long cluster_reference_index = cluster_partition_port_add(
						partition->cluster_reference_data,
						first_cluster_reference,
						c_idx,
						(word)other_bsp,
						NULL);
					long data_reference_index = cluster_partition_port_add(
						partition->data_reference_data,
						cluster_partition_get_first_reference_for_bsp(partition, other_bsp, c_idx),
						datum_index,
						data_pad,
						partition->port_previous_references);

					partition->port_modification_count++;
					if (multi_bsp_state)
						multi_bsp_state->synced_modification_count = partition->port_modification_count;
					if (cluster_reference_index != NONE && partition->port_cluster_data_references)
					{
						partition->port_cluster_data_references[DATUM_INDEX_TO_ABSOLUTE_INDEX(cluster_reference_index)] =
							data_reference_index;
					}
				}
			}
		}

		if (switched_bsp)
		{
			scenario_set_active_bsp_context(primary_bsp);
		}
	}

	return;
}

void cluster_partition_disconnect(
	struct cluster_partition *partition,
	long datum_index,
	long *first_cluster_reference)
{
	long cluster_reference_index = *first_cluster_reference;

	while (cluster_reference_index != NONE)
	{
		struct data_reference *cluster_reference = (struct data_reference *)datum_get(
			partition->cluster_reference_data,
			cluster_reference_index);
		short const cluster_index = (short)cluster_reference->datum_index;
		long data_reference_index = partition->port_cluster_data_references ?
			partition->port_cluster_data_references[DATUM_INDEX_TO_ABSOLUTE_INDEX(cluster_reference_index)] : NONE;
		short bsp_index = (short)cluster_reference->pad;

		if (bsp_index < 0 || bsp_index >= MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO)
			bsp_index = scenario_get_active_bsp_index();

		datum_delete(partition->cluster_reference_data, cluster_reference_index);

		/* port: reference_list_remove's, starting where the reference was
		put */
		cluster_partition_port_remove(
			partition,
			cluster_partition_get_first_reference_for_bsp(partition, bsp_index, cluster_index),
			datum_index,
			data_reference_index);

		cluster_reference_index = cluster_reference->next_reference_index;
	}

	*first_cluster_reference = NONE;

	return;
}

long cluster_partition_get_first_datum(
	struct cluster_partition const *partition,
	long *reference_index,
	short cluster_index)
{
	*reference_index = *cluster_partition_get_first_reference((struct cluster_partition *)partition, cluster_index);

	return reference_list_get_next_datum_index(partition->data_reference_data, reference_index);
}

/* ---------- private code */

/* port: Where each reference is in its cluster's list, so a datum leaving
a cluster is found there at once. Every moving object and light leaves its
clusters and joins them again each tick (reference_list_add puts it first),
and reference_list_remove walked the list from its start: with hundreds of
lights in a few clusters (network co-op's extra enemies) those walks took
much of the tick. Each reference's place is checked before it is used, and
the list walked as before when it is wrong: the lists come out as they did,
every reference in the same place. The places are not in the game state, so
they are forgotten when it is loaded (cluster_partitions_port_forget) or a
new map makes the lists anew. */

static void cluster_partition_port_forget(
	struct cluster_partition *partition)
{
	partition->port_modification_count++;
	if (partition->port_previous_references)
		csmemset(partition->port_previous_references, 0xFF, HALO_PORT_MAXIMUM_CLUSTER_REFERENCES * sizeof(long));
	if (partition->port_cluster_data_references)
		csmemset(partition->port_cluster_data_references, 0xFF, HALO_PORT_MAXIMUM_CLUSTER_REFERENCES * sizeof(long));
}

/* port: rebuild per-BSP cluster list heads and previous-reference links from
partition->data_reference_data after cluster_partition_port_forget or a game_state load/revert */
static void cluster_partition_multi_bsp_resync(
	struct cluster_partition *partition,
	struct cluster_partition_multi_bsp_state *state)
{
	short bsp_slot;
	struct data_iterator iterator;
	struct data_reference *reference;

	for (bsp_slot = 1; bsp_slot < MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO; bsp_slot++)
	{
		csmemset(
			state->bsp_cluster_first_data_references[bsp_slot],
			NONE,
			sizeof(state->bsp_cluster_first_data_references[bsp_slot]));
	}
	if (partition->data_reference_data &&
		partition->data_reference_data->valid &&
		partition->data_reference_data->actual_count > 0)
	{
		if (partition->port_previous_references)
		{
			data_iterator_new(&iterator, partition->data_reference_data);
			while ((reference = data_iterator_next(&iterator)) != NULL)
			{
				if (reference->next_reference_index != NONE)
				{
					short next_abs = DATUM_INDEX_TO_ABSOLUTE_INDEX(reference->next_reference_index);

					if (next_abs >= 0 && next_abs < HALO_PORT_MAXIMUM_CLUSTER_REFERENCES)
						partition->port_previous_references[next_abs] = iterator.datum_index;
				}
			}
		}

		data_iterator_new(&iterator, partition->data_reference_data);
		while ((reference = data_iterator_next(&iterator)) != NULL)
		{
			short abs_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);

			if (reference->pad != 0xFFFF &&
				(!partition->port_previous_references ||
					(abs_index >= 0 && abs_index < HALO_PORT_MAXIMUM_CLUSTER_REFERENCES &&
						partition->port_previous_references[abs_index] == NONE)))
			{
				short bsp_index = (short)((reference->pad >> 9) & 0x1F);
				short cluster_index = (short)(reference->pad & 0x1FF);

				if (bsp_index > 0 &&
					bsp_index < MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO &&
					cluster_index >= 0 &&
					cluster_index < MAXIMUM_CLUSTERS_PER_STRUCTURE)
				{
					state->bsp_cluster_first_data_references[bsp_index][cluster_index] = iterator.datum_index;
				}
			}
		}
	}
	state->synced_modification_count = partition->port_modification_count;
}

static struct cluster_partition_multi_bsp_state *cluster_partition_multi_bsp_get(
	struct cluster_partition *partition)
{
	short index;

	for (index = 0; index < MAXIMUM_PORT_CLUSTER_PARTITIONS; index++)
	{
		if (cluster_partitions_multi_bsp[index].partition == partition)
		{
			if (cluster_partitions_multi_bsp[index].synced_modification_count != partition->port_modification_count)
				cluster_partition_multi_bsp_resync(partition, &cluster_partitions_multi_bsp[index]);
			return &cluster_partitions_multi_bsp[index];
		}
		if (!cluster_partitions_multi_bsp[index].partition)
		{
			cluster_partitions_multi_bsp[index].partition = partition;
			cluster_partition_multi_bsp_resync(partition, &cluster_partitions_multi_bsp[index]);
			return &cluster_partitions_multi_bsp[index];
		}
	}

	return NULL;
}

/* reference_list_add's, and the new reference's place: first, before the
one that was (previous_references, if given) */
static long cluster_partition_port_add(
	struct data_array *array,
	long *first_reference_index,
	long datum_index,
	word pad_tag,
	long *previous_references)
{
	long reference_index = datum_new(array);

	if (reference_index != NONE)
	{
		struct data_reference *reference = (struct data_reference *)datum_get(array, reference_index);

		reference->pad = pad_tag;
		reference->datum_index = datum_index;
		reference->next_reference_index = *first_reference_index;
		if (previous_references)
		{
			previous_references[DATUM_INDEX_TO_ABSOLUTE_INDEX(reference_index)] = NONE;
			if (*first_reference_index != NONE)
				previous_references[DATUM_INDEX_TO_ABSOLUTE_INDEX(*first_reference_index)] = reference_index;
		}
		*first_reference_index = reference_index;
	}
	else
	{
		/* port: once a map, the rest counted (reference_list_port_full) */
		reference_list_port_full(array);
	}

	return reference_index;
}

/* reference_list_remove's on a cluster's list, at found_reference_index
(where the datum's reference was put) if it is there */
static void cluster_partition_port_remove(
	struct cluster_partition *partition,
	long *first_reference_index,
	long datum_index,
	long found_reference_index)
{
	struct data_array *array = partition->data_reference_data;
	long *previous_references = partition->port_previous_references;
	long reference_index = NONE;
	long previous_index = NONE;
	struct data_reference *reference;
	long next_index;
	struct cluster_partition_multi_bsp_state *multi_bsp_state = cluster_partition_multi_bsp_get(partition);

	/* (the reference put there, still this datum's, and first in this
	cluster's list or after the one noted before it) */
	if (previous_references && found_reference_index != NONE)
	{
		reference = (struct data_reference *)datum_try_and_get(array, found_reference_index);
		if (reference && reference->datum_index == datum_index)
		{
			long noted_previous = previous_references[DATUM_INDEX_TO_ABSOLUTE_INDEX(found_reference_index)];
			struct data_reference *previous = noted_previous != NONE ?
				(struct data_reference *)datum_try_and_get(array, noted_previous) : NULL;

			if (noted_previous == NONE ? *first_reference_index == found_reference_index :
				previous && previous->next_reference_index == found_reference_index)
			{
				reference_index = found_reference_index;
				previous_index = noted_previous;
			}
		}
	}
	/* (else the list walked, as reference_list_remove does) */
	if (reference_index == NONE)
	{
		long index = *first_reference_index;

		while (index != NONE)
		{
			reference = (struct data_reference *)datum_get(array, index);
			if (reference->datum_index == datum_index)
			{
				reference_index = index;
				break;
			}
			previous_index = index;
			index = reference->next_reference_index;
		}
	}
	if (reference_index == NONE)
	{
		match_vassert(
			"..\\objects\\reference_lists.h",
			0x6d,
			FALSE,
			csprintf(temporary, "attempt to remove invalid element %ld from reference list", datum_index));
		return;
	}
	partition->port_modification_count++;
	if (multi_bsp_state)
		multi_bsp_state->synced_modification_count = partition->port_modification_count;
	reference = (struct data_reference *)datum_get(array, reference_index);
	next_index = reference->next_reference_index;
	datum_delete(array, reference_index);
	if (previous_index == NONE)
		*first_reference_index = next_index;
	else
		((struct data_reference *)datum_get(array, previous_index))->next_reference_index = next_index;
	if (previous_references && next_index != NONE)
		previous_references[DATUM_INDEX_TO_ABSOLUTE_INDEX(next_index)] = previous_index;
}

static long *cluster_partition_get_first_reference_for_bsp(
	struct cluster_partition *partition,
	short bsp_index,
	short cluster_index)
{
	struct cluster_partition_multi_bsp_state *multi_bsp_state = cluster_partition_multi_bsp_get(partition);
	long *first_references = partition->cluster_first_data_references;

	if (cluster_index < 0 || cluster_index >= MAXIMUM_CLUSTERS_PER_STRUCTURE)
	{
		cluster_partition_port_no_cluster_first_reference = NONE;
		return &cluster_partition_port_no_cluster_first_reference;
	}
	if (bsp_index > 0 &&
		bsp_index < MAXIMUM_STRUCTURE_BSPS_PER_SCENARIO &&
		multi_bsp_state)
	{
		first_references = multi_bsp_state->bsp_cluster_first_data_references[bsp_index];
	}

	return &first_references[cluster_index];
}

static long *cluster_partition_get_first_reference(
	struct cluster_partition *partition,
	short cluster_index)
{
	return cluster_partition_get_first_reference_for_bsp(
		partition,
		scenario_get_active_bsp_index(),
		cluster_index);
}
