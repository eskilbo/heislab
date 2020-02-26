#include <stdio.h>
#include <stdlib.h>
#include "hardware.h"
#include "elevator.h"
#include "timer.h"

int FLOOR_COUNT = 4;
int BUTTON_COUNT = 3;

static int current_floor = 0;
static int current_direction = HARDWARE_MOVEMENT_STOP;

int elevator_queue[4];

CurrentState state = IDLE;

//CurrentDestination dest = FIRST;

int elevator_get_current_floor()
{
    for (int i = 0; i < FLOOR_COUNT; i++)
    {
        if (hardware_read_floor_sensor(i))
        {
            return i + 1;
        }
    }
    return 0;
}

void elevator_set_current_floor()
{
    if (elevator_get_current_floor())
    {
        current_floor = elevator_get_current_floor() - 1;
    }
}

void print_elevator_queue()
{
    for (int i = 0; i < FLOOR_COUNT; ++i)
    {
        printf("Floor %d, has index %d \n", i + 1, elevator_queue[i]);
    }
}

void order_buttons_up()
{
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        if (hardware_read_order(i, HARDWARE_ORDER_UP))
        {
            hardware_command_order_light(i, HARDWARE_ORDER_UP, 1);
            if (elevator_queue[i] == BOTH_OR_INSIDE || elevator_queue[i] == DOWN)
            {
                elevator_queue[i] = BOTH_OR_INSIDE;
            }
            else
            {
                elevator_queue[i] = UP;
            }
        }
    }
}

void clear_order_buttons_up()
{
    for (int i = 0; i < BUTTON_COUNT; ++i)
    {
        if (elevator_queue[i] == 0 || elevator_queue[i] == 2)
        {
            hardware_command_order_light(i, HARDWARE_ORDER_UP, 0);
        }
    }
}

void order_buttons_down()
{
    for (int i = 1; i < BUTTON_COUNT + 1; ++i)
    {
        if (hardware_read_order(i, HARDWARE_ORDER_DOWN))
        {
            hardware_command_order_light(i, HARDWARE_ORDER_DOWN, 1);
            if (elevator_queue[i] == BOTH_OR_INSIDE || elevator_queue[i] == UP)
            {
                elevator_queue[i] = BOTH_OR_INSIDE;
            }
            else
            {
                elevator_queue[i] = DOWN;
            }
        }
    }
}

void clear_order_buttons_down()
{
    for (int i = 1; i < BUTTON_COUNT + 1; ++i)
    {
        if (elevator_queue[i] == 0 || elevator_queue[i] == 1)
        {
            hardware_command_order_light(i, HARDWARE_ORDER_DOWN, 0);
        }
    }
}

void order_buttons_inside()
{
    for (int i = 0; i < BUTTON_COUNT + 1; ++i)
    {
        if (hardware_read_order(i, HARDWARE_ORDER_INSIDE))
        {
            hardware_command_order_light(i, HARDWARE_ORDER_INSIDE, 1);
            elevator_queue[i] = BOTH_OR_INSIDE;
            //printf("Inside button on floor %d pressed ", i);
        }
    }
}

void clear_order_buttons_inside()
{
    for (int i = 0; i < BUTTON_COUNT + 1; ++i)
    {
        if (elevator_queue[i] != 3)
        {
            hardware_command_order_light(i, HARDWARE_ORDER_INSIDE, 0);
        }
    }
}

void stop_button()
{
    if (hardware_read_stop_signal())
    {
        hardware_command_stop_light(1);
        state = EMERGENCY_STOP;
    }
}

void floor_lights()
{
    //printf("Current floor: %d ", current_floor);
    hardware_command_floor_indicator_on(current_floor);
}

void handle_buttons()
{
    order_buttons_up();
    clear_order_buttons_up();
    order_buttons_down();
    clear_order_buttons_down();
    order_buttons_inside();
    clear_order_buttons_inside();
    stop_button();
    floor_lights();
}

void clear_all_orders()
{
    for (int i = 0; i < FLOOR_COUNT; ++i)
    {
        elevator_queue[i] = 0;
    }
}

int get_destination()
{
    int min_distance = FLOOR_COUNT;
    int current_distance = 0;
    int current_destination = -1;
    for (int i = 0; i < FLOOR_COUNT; ++i)
    {
        if (elevator_queue[i])
        {
            current_distance = abs(current_floor - i);
            if (current_distance <= min_distance)
            {
                if ((current_direction == HARDWARE_MOVEMENT_UP && i > current_floor && (elevator_queue[i] == UP || elevator_queue[i] == BOTH_OR_INSIDE)) ||
                    (current_direction == HARDWARE_MOVEMENT_DOWN && i < current_floor && (elevator_queue[i] == DOWN || elevator_queue[i] == BOTH_OR_INSIDE)) ||
                    current_direction == HARDWARE_MOVEMENT_STOP)
                {
                    min_distance = current_distance;
                    current_destination = i;
                }
            }
        }
    }
    printf("Destination in get_dest() %d \n", current_destination);
    set_direction(current_destination);
    return current_destination;
}

void set_direction(int destination){
    if (destination == -1)
    {
        current_direction = HARDWARE_MOVEMENT_STOP;
    }
    else if (destination > current_floor)
    {
        current_direction = HARDWARE_MOVEMENT_UP;
    }
    else if (destination < current_floor)
    {
        current_direction = HARDWARE_MOVEMENT_DOWN;
    } else if (destination == current_floor){
        current_direction = HARDWARE_MOVEMENT_STOP;
    }
}

void elevator_startup()
{
    // Initalize hardware
    int error = hardware_init();
    if (error != 0)
    {
        fprintf(stderr, "Unable to initialize hardware\n");
        exit(1);
    }

    // Move to first floor
    while (elevator_get_current_floor() != 1)
    {
        hardware_command_movement(HARDWARE_MOVEMENT_DOWN);
    }
    hardware_command_movement(HARDWARE_MOVEMENT_STOP);
}

void travel_to_destination(int destination_floor)
{
    while (get_destination() != current_floor)
    {
        handle_buttons();
        elevator_set_current_floor();
        hardware_command_movement(current_direction);
        //printf("Travelling to floor %d, with the direction %d \n", destination_floor, current_direction);
        if (hardware_read_stop_signal())
        {
            state = EMERGENCY_STOP;
            break;
        }
    }
    hardware_command_movement(HARDWARE_MOVEMENT_STOP);
}

void run_elevator()
{
    handle_buttons();
    elevator_startup();
    int destination = 0;

    while (1)
    {
        handle_buttons();
        elevator_set_current_floor();
        printf("Current state: %d \n", state);
        //printf("UP: %d ", UP);
        switch (state)
        {
        case IDLE:
            hardware_command_movement(HARDWARE_MOVEMENT_STOP);
            hardware_command_door_open(0);
            current_direction = HARDWARE_MOVEMENT_STOP;
            for (int i = 0; i < FLOOR_COUNT; ++i)
            {
                if (elevator_queue[i])
                {
                    if (elevator_get_current_floor() == i + 1)
                    {
                        state = DOOR;
                    }
                    else
                    {
                        state = RUNNING;
                    }
                }
            }
            printf("Direction in IDLE: %d \n", current_direction);
            break;

        case RUNNING:
        /*
            printf("Destination is floor %d \n", get_destination());
            destination = get_destination();
            printf("Direction in RUNNING: %d \n", current_direction);
            if (destination != -1)
            {
                //printf("Travelling to floor %d, with the direction %d \n", destination, current_direction);
                travel_to_destination(destination);
                state = DOOR;
            }
            else
            {
                state = IDLE;
            }
            */
            for (int i = 0; i < 4; ++i){
                if (elevator_queue[i]== 1 || elevator_queue[i]== 3){
                    while(current_floor != i){
                    handle_buttons();
                    set_current_floor();
                    if (i < current_floor){
                        hardware_movement(HARDWARE_MOVEMENT_DOWN);
                    } else if (i > current_floor){
                        hardware_movement(HARDWARE_MOVEMENT_UP);
                    }
                    
                    }
                    state = DOOR;
                    hardware_movement(HARDWARE_MOVEMENT_STOP);
                    }
                }

                for (int i = 3; i > -1; --i){
                if (elevator_queue[i]== 2 || elevator_queue[i]== 3){
                    while(current_floor != i){
                    hardware_movement(HARDWARE_MOVEMENT_DOWN);
                    if (i < current_floor){
                        hardware_movement(HARDWARE_MOVEMENT_DOWN);
                    } else if (i > current_floor){
                        hardware_movement(HARDWARE_MOVEMENT_UP);
                        }
                    }
                    state = DOOR;
                    hardware_movement(HARDWARE_MOVEMENT_STOP);
                }
            }
            break;

        case DOOR:
            hardware_command_door_open(1);
            elevator_queue[current_floor] = 0;
            handle_buttons();
            timer_start_timer(3000);

            int counter = 0;
            for (int i = 0; i < FLOOR_COUNT; ++i){
                if(!elevator_queue[i]){
                    counter++;
                }
            }
            if (counter == FLOOR_COUNT){
                current_direction = HARDWARE_MOVEMENT_STOP;
            }
            while (!timer_check_expired())
            {
                handle_buttons();
            }
            if (hardware_read_obstruction_signal())
            {
                state = OBSTRUCT;
            }
            else
            {
                hardware_command_door_open(0);
                for (int i = 0; i < FLOOR_COUNT; ++i)
                {
                    if (elevator_queue[i])
                    {
                        if (elevator_get_current_floor() == i + 1)
                        {
                            state = DOOR;
                        }
                        else
                        {
                            get_destination();
                            state = RUNNING;
                        }
                    }
                    else
                    {
                        state = IDLE;
                    }
                }
            }
            break;
        case EMERGENCY_STOP:
            hardware_command_movement(HARDWARE_MOVEMENT_STOP);
            if (elevator_get_current_floor())
            {
                while (hardware_read_stop_signal())
                {
                    hardware_command_door_open(1);
                }
                timer_start_timer(3000);
                while (!timer_check_expired())
                {
                }
                hardware_command_door_open(0);
            }
            clear_all_orders();
            hardware_command_stop_light(0);
            state = IDLE;
            break;

        case OBSTRUCT:
            if (elevator_get_current_floor())
            {
                timer_start_timer(3000);
                while (!timer_check_expired())
                {
                    handle_buttons();
                }
                state = DOOR;
            }
            else
            {
                state = IDLE;
            }
            break;
        }
    }
}