/* ROM-free compile-only fixture for native CI tests.
 *
 * Production builds generate assets/ge_animation_offsets.h from a user-supplied ROM
 * via tools/gen_anim_blobs.py. CI only needs these symbol names to compile patched
 * translation units; these dummy values are never executed.
 *
 * Deliberately use zero rather than publishing ROM-derived offset data.
 */
#ifndef GE_TEST_ANIMATION_OFFSETS_H
#define GE_TEST_ANIMATION_OFFSETS_H

#define GE_ANIMOFF_ANIM_DATA_bond_eye_fire 0u
#define GE_ANIMOFF_ANIM_DATA_bond_eye_walk 0u
#define GE_ANIMOFF_ANIM_DATA_bond_watch 0u
#define GE_ANIMOFF_ANIM_DATA_death_left_leg 0u
#define GE_ANIMOFF_ANIM_DATA_death_neck 0u
#define GE_ANIMOFF_ANIM_DATA_death_stagger_back_to_wall 0u
#define GE_ANIMOFF_ANIM_DATA_extending_left_hand 0u
#define GE_ANIMOFF_ANIM_DATA_fire_hip 0u
#define GE_ANIMOFF_ANIM_DATA_fire_jump_to_side_left 0u
#define GE_ANIMOFF_ANIM_DATA_fire_jump_to_side_right 0u
#define GE_ANIMOFF_ANIM_DATA_fire_kneel_forward_one_handed_weapon_slow 0u
#define GE_ANIMOFF_ANIM_DATA_fire_kneel_left_leg 0u
#define GE_ANIMOFF_ANIM_DATA_fire_standing_draw_one_handed_weapon_fast 0u
#define GE_ANIMOFF_ANIM_DATA_fire_throw_grenade 0u
#define GE_ANIMOFF_ANIM_DATA_hit_butt_long 0u
#define GE_ANIMOFF_ANIM_DATA_hit_butt_short 0u
#define GE_ANIMOFF_ANIM_DATA_idle 0u
#define GE_ANIMOFF_ANIM_DATA_idle_unarmed 0u
#define GE_ANIMOFF_ANIM_DATA_jump_backwards 0u
#define GE_ANIMOFF_ANIM_DATA_look_around 0u
#define GE_ANIMOFF_ANIM_DATA_running 0u
#define GE_ANIMOFF_ANIM_DATA_running_female 0u
#define GE_ANIMOFF_ANIM_DATA_running_one_handed_weapon 0u
#define GE_ANIMOFF_ANIM_DATA_side_step_left 0u
#define GE_ANIMOFF_ANIM_DATA_slide_left 0u
#define GE_ANIMOFF_ANIM_DATA_slide_right 0u
#define GE_ANIMOFF_ANIM_DATA_sneeze 0u
#define GE_ANIMOFF_ANIM_DATA_spotting_bond 0u
#define GE_ANIMOFF_ANIM_DATA_sprinting 0u
#define GE_ANIMOFF_ANIM_DATA_sprinting_one_handed_weapon 0u
#define GE_ANIMOFF_ANIM_DATA_surrendering_armed 0u
#define GE_ANIMOFF_ANIM_DATA_surrendering_armed_drop_weapon 0u
#define GE_ANIMOFF_ANIM_DATA_walking 0u
#define GE_ANIMOFF_ANIM_DATA_walking_female 0u
#define GE_ANIMOFF_ANIM_DATA_walking_unarmed 0u

#endif
