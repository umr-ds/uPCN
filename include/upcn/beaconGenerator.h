#ifndef BEACON_GENERATOR_H_INCLUDED
#define BEACON_GENERATOR_H_INCLUDED

#ifndef ND_DISABLE_BEACONS

enum upcn_result beacon_generator_init(void);
uint64_t beacon_generator_check_send(uint64_t cur_time);
void beacon_generator_reset_next_time(void);

#else

#define beacon_generator_reset_next_time() ((void)0)

#endif /* ND_DISABLE_BEACONS */

#endif /* BEACON_GENERATOR_H_INCLUDED */
