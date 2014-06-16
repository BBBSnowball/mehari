#ifndef DEBUG_PRINT_H
#define DEBUG_PRINT_H

//#define DRY_RUN
//#define ENABLE_DEBUG_PRINT


#ifdef ENABLE_DEBUG_PRINT
# define debug_print(x) (std::cout << x << std::endl)
#else
# define debug_print(x)
#endif

#ifdef DRY_RUN
# define return_if_dry_run() return
# define return_value_if_dry_run(x) return (x)
#else
# define return_if_dry_run()
# define return_value_if_dry_run(x)
#endif

#endif /*DEBUG_PRINT_H*/
