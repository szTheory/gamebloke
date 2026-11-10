#ifndef GAMEBLOKE_GAMEBLOKE_H
#define GAMEBLOKE_GAMEBLOKE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the project version string (for example "0.0.0"). The string is
 * static; the caller must not free or modify it.
 *
 * No lifecycle API exists yet; Phases 3 and 4 define it. */
const char *gbl_version(void);

#ifdef __cplusplus
}
#endif

#endif /* GAMEBLOKE_GAMEBLOKE_H */
