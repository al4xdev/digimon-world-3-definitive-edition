/* The expansion save's sections (definitive_expansion.h), in a fixed order: the savestate's. A new one is appended
 * (.plan/tasks/expansion-save.md "How to add a section"); none is ever removed or renamed while a save may have it. */
#include <stddef.h>

#include "definitive_expansion.h"

extern const DefExpansionSection def_persona_section;
extern const DefExpansionSection def_respec_section;
extern const DefExpansionSection def_bestiary_ext_section;

const DefExpansionSection *const def_expansion_sections[] = {
    &def_persona_section,      /* the enemy personas' discovery (definitive_persona_save.c) */
    &def_respec_section,       /* the Velvet Room pig's swaps paid, for the climbing price (definitive_respec_save.c) */
    &def_bestiary_ext_section, /* the extended 3D bestiary catalog discovery (definitive_bestiary_ext.c) */
    NULL,
};
