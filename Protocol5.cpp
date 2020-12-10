#include "protocol.h"

boolean NetworkLayer = false;
packet NetworkLayer_Buffer[8];
seq_nr ack_expected; /* oldest frame as yet unacknowledged */ // serial to recieve
int TimeOutFlag = 0;
unsigned int client_socket;
unsigned int server_socket;
void Set_Timer_Out(seq_nr arg)
{
    TimeOutFlag = 1;
}
void create_timer(long period, void (*func)(seq_nr), void *ptr_val)
{
    timer_t timer_id;
    struct itimerspec ts;
    struct sigevent se;

    se.sigev_notify = SIGEV_THREAD;
    se.sigev_value.sival_ptr = ptr_val;
    se.sigev_notify_function = func;
    se.sigev_notify_attributes = NULL;

    ts.it_value.tv_sec = period / 1000;
    ts.it_value.tv_nsec = (period % 1000) * 1000000;
    ts.it_interval.tv_sec = period / 1000;
    ts.it_interval.tv_nsec = (period % 1000) * 1000000;

    timer_create(CLOCK_REALTIME, &se, &timer_id);

    timer_settime(timer_id, 0, &ts, 0);
}
void Connect_Slave(void)
{
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("192.168.1.8");
    address.sin_port = htons(PORT);
    printf("Waiting for Connection...\n");
    while (connect(client_socket, (struct sockaddr *)&address, sizeof(address)))
        ;
    printf("Slave Connected\n");
}
void Connect_Master(void)
{
    printf("Master Waiting\n");
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("192.168.1.8");
    address.sin_port = htons(PORT);
    bind(server_socket, (struct sockaddr *)&address, sizeof(address));
    listen(server_socket, 1);
    client_socket = accept(server_socket, NULL, NULL);
    printf("Master connected\n");
}
/* Wait for an event to happen; return its type in event. */
void wait_for_event(event_type *event)
{
    if (NetworkLayer)
        *event = network_layer_ready;
    else if (TimeOutFlag == 1)
        *event = timeout;
    else
    {
        *event = frame_arrival;
    }
}
/* Fetch a packet from the network layer for transmission on the channel. */
void from_network_layer(packet *p)
{
    static char i = 0;
    for (char j = 0; j < 8; j++)
        p->data[j] = NetworkLayer_Buffer[i].data[j];

    i = (i + 1) % 8;
}

/* Deliver information from an inbound frame to the network layer.*/
void to_network_layer(packet *p)
{
    static int m = 0;
    for (char j = 0; j < 8; j++)
        NetworkLayer_Buffer[m].data[j] = p->data[j];
    m = (m + 1) % 8;
}
/* Go get an inbound frame from the physical_layer and copy it to r. */
void from_physical_layer(frame *r)
{
    read(client_socket, r, sizeof(frame));
    printf("From physical layer \n");
     printf("info is : ");
    for(int i = 0; i< 8; i++)
    {
        printf("%d  ", r->info.data[i]);
    }
    printf("\n");
}
/* Pass the frame to the physical_layer for transmission. */
void to_physical_layer(frame *s)
{
    write(client_socket, s, sizeof(frame));
}
/* Start the clock running and enable the timeout event.*/
void start_timer(seq_nr k)
{
    TimeOutFlag = 0;
    create_timer(2000, Set_Timer_Out, k);
}
/* Stop the clock and disable the timeout event.*/
void stop_timer(seq_nr k)
{
}

/*Start an auxiliary timer and enable the ack timeout event.*/
//void start_ack_timer(void);

/* Stop the auxiliary timer and disable the ack timeout event.*/
//void stop_ack_timer(void);

/* Allow the network layer to cause a network layer ready event.*/
void enable_network_layer(void)
{
    NetworkLayer = true;
}
/* Forbid the network layer from causing a network layer ready event. */
void disable_network_layer(void)
{
    NetworkLayer = false;
}
static boolean between(seq_nr a, seq_nr b, seq_nr c)
{
    /* Return true if a <=b < c circularly; false otherwise. */
    if (((a <= b) && (b < c)) || ((c < a) && (a <= b)) || ((b < c) && (c < a)))
        return (true);
    else
        return (false);
}
static void send_data(seq_nr frame_nr, seq_nr frame_expected, packet buffer[])
{
    /* Construct and send a data frame. */
    frame s;                                            /* scratch variable */
    s.info = buffer[frame_nr];                          /* insert packet into frame */
    s.seq = frame_nr;                                   /* insert sequence number into frame */
    s.ack = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1); /* piggyback ack */
    to_physical_layer(&s);                              /* transmit the frame */
                                                        // start_timer(frame_nr);                              /* start the timer running */
}
void protocol5(void)
{
    seq_nr next_frame_to_send; /* MAX SEQ > 1; used for outbound stream */ // seral to send
    seq_nr frame_expected;                                                 /* next frame expected on inbound stream */
    frame r;                                                               /* scratch variable */
    packet buffer[MAX_SEQ + 1];                                            /* buffers for the outbound stream */
    seq_nr nbuffered;                                                      /* number of output buffers currently in use */
    seq_nr i;                                                              /* used to index into the buffer array */
    event_type event;
    enable_network_layer(); /* allow network layer ready events */
    ack_expected = 0;       /* next ack expected inbound */
    next_frame_to_send = 0; /* next frame going out */
    frame_expected = 0;     /* number of frame expected inbound */
    nbuffered = 0;          /* initially no packets are buffered */
    Connect_Slave();
    while (1)
    {
        wait_for_event(&event); /* four possibilities: see event type above */
        switch (event)
        {
        case network_layer_ready: /* the network layer has a packet to send */
            /* Accept, save, and transmit a new frame. */
            from_network_layer(&buffer[next_frame_to_send]);       /* fetch new packet */
            nbuffered = nbuffered + 1;                             /* expand the sender’s window */
            send_data(next_frame_to_send, frame_expected, buffer); /* transmit the frame */
            inc(next_frame_to_send);                               /* advance sender’s upper window edge */
            break;
        case frame_arrival:          /* a data or control frame has arrived */
            from_physical_layer(&r); /* get incoming frame from physical layer */
            if (r.seq == frame_expected)
            {
                /* Frames are accepted only in order. */
                to_network_layer(&r.info); /* pass packet to network layer */
                inc(frame_expected);       /* advance lower edge of receiver’s window */
            }
            /* Ack n implies n − 1, n − 2, etc. Check for this. */
            while (between(ack_expected, r.ack, next_frame_to_send))
            {
                /* Handle piggybacked ack. */
                nbuffered = nbuffered - 1; /* one frame fewer buffered */
                stop_timer(ack_expected);  /* frame arrived intact; stop timer */
                inc(ack_expected);         /* contract sender’s window */
            }
            break;
        case cksum_err:
            break;                             /* just ignore bad frames */
        case timeout:                          /* trouble; retransmit all outstanding frames */
            next_frame_to_send = ack_expected; /* start retransmitting here */
            for (i = 1; i <= nbuffered; i++)
            {
                send_data(next_frame_to_send, frame_expected, buffer); /* resend frame */
                inc(next_frame_to_send);                               /* prepare to send the next one */
            }
        }
        if (nbuffered < MAX_SEQ)
            enable_network_layer();
        else
            disable_network_layer();
    }
}
int main()
{
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            NetworkLayer_Buffer[i].data[j] = j + 1;
    protocol5();

    return 0;
}
