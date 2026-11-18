/* Prints the host layout of LIBSND's sequencer so a debug poke can retarget a
 * playing sequence. Compiled against the port headers; not part of the game. */
#include <stddef.h>
#include <stdio.h>

#include "libsnd_internal.h"

#define F(type, field) printf("  \"%s.%s\": %zu,\n", #type, #field, offsetof(type, field))

int main(void) {
    printf("{\n");
    printf("  \"SndSeq\": %zu,\n", sizeof(SndSeq));
    printf("  \"SndState\": %zu,\n", sizeof(SndState));
    printf("  \"SndVab\": %zu,\n", sizeof(SndVab));
    printf("  \"SndVoice\": %zu,\n", sizeof(SndVoice));
    F(SndState, s_max);
    F(SndState, t_max);
    F(SndState, seq);
    F(SndState, vab);
    F(SndState, nvoices);
    F(SndState, voice);
    F(SndState, koff);
    F(SndSeq, pos);
    F(SndSeq, start);
    F(SndSeq, loop);
    F(SndSeq, flags);
    F(SndSeq, play_mode);
    F(SndSeq, status);
    F(SndSeq, channel);
    F(SndSeq, loop_count);
    F(SndSeq, play_count);
    F(SndSeq, played);
    F(SndSeq, vab);
    F(SndSeq, pan);
    F(SndSeq, prog);
    F(SndSeq, ch_vol);
    F(SndSeq, mute);
    F(SndSeq, resolution);
    F(SndSeq, frame_wait);
    F(SndSeq, step);
    F(SndSeq, vol_l);
    F(SndSeq, first_delta);
    F(SndSeq, bpm);
    F(SndSeq, delta);
    F(SndVab, state);
    F(SndVab, head);
    F(SndVab, nprogs);
    F(SndVoice, channel);
    F(SndVoice, note);
    F(SndVoice, vel);
    F(SndVoice, owner);
    F(SndVoice, prog);
    F(SndVoice, status);
    printf("  \"SND_SEQ_PLAYING\": %d\n", SND_SEQ_PLAYING);
    printf("}\n");
    return 0;
}
