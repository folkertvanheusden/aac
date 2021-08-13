// (c) 2021 by Folkert van Heusden <mail@vanheusden.com>
// Release under BSD 3-Clause license
#include <stdio.h>

#include <alsa/asoundlib.h>

void open_client(snd_seq_t **const handle, int *const port)
{
	int err = snd_seq_open(handle, "default", SND_SEQ_OPEN_INPUT, 0);
	if (err < 0) {
		perror("snd_seq_open");
		exit(1);
	}

	snd_seq_set_client_name(*handle, "AAC");

	*port = snd_seq_create_simple_port(*handle, "---", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC);
}

void alsa_processor(snd_seq_t *const seq, const int tgt_client, const int tgt_port)
{
	for(;;) {
		int rc = 0;
		snd_seq_event_t *ev = nullptr;
		if ((rc = snd_seq_event_input(seq, &ev)) < 0) {
			printf("snd_seq_event_input failed: %s\n", strerror(-rc));
			break;
		}

		if (ev->type == SND_SEQ_EVENT_PORT_START) {
			printf("Connecting %d:%d to %d:%d\n", ev->data.addr.client, ev->data.addr.port, tgt_client, tgt_port);

			snd_seq_addr_t tgt, src;
			tgt.client = tgt_client;
			tgt.port = tgt_port;
			src.client = ev->data.addr.client;
			src.port = ev->data.addr.port;
			snd_seq_port_subscribe_t *subs = nullptr;
			snd_seq_port_subscribe_alloca(&subs);
			snd_seq_port_subscribe_set_sender(subs, &src);
			snd_seq_port_subscribe_set_dest(subs, &tgt);
			snd_seq_port_subscribe_set_queue(subs, 1);
			snd_seq_port_subscribe_set_time_update(subs, 1);
			snd_seq_port_subscribe_set_time_real(subs, 1);
			snd_seq_subscribe_port(seq, subs);
		}
		else {
			printf("Unexpected event type: %d\n", ev->type);
		}
	}
}

void setup_autoconnect(snd_seq_t *const seq, const int port)
{
	int err = snd_seq_connect_from(seq, port, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);
	if (err < 0) {
		printf("Cannot subscribe to announce port: %s\n", strerror(-err));
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s destination_client:port\n", argv[0]);
		return 1;
	}

	int local_port = -1;
  	snd_seq_t *seq = nullptr;
	open_client(&seq, &local_port);

	setup_autoconnect(seq, local_port);

	int tgt_client = atoi(argv[1]), tgt_port = 0;
	char *col = strchr(argv[1], ':');
	if (col)
		tgt_port = atoi(col + 1);

	printf("Will connect new sources to %d:%d\n", tgt_client, tgt_port);

	alsa_processor(seq, tgt_client, tgt_port);

	snd_seq_close(seq);

	return 0;
}
