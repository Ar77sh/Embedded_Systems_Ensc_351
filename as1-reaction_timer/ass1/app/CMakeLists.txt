add_executable(reaction_timer
    src/reaction_timer.c
)

target_link_libraries(reaction_timer PRIVATE hal)
