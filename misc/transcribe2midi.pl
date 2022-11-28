#!/usr/bin/env perl
use 5.036;

package NoteEvent {
    use Moo;
    use Types::Standard qw(Int Num);

    has start       => (is => 'ro', isa => Int, required => 1);
    has finish      => (is => 'ro', isa => Int, required => 1);
    has velocity    => (is => 'ro', isa => Num, required => 1);
}

package TransientDetector {
    use Moo;
    use Types::Standard qw(ArrayRef Int Num Object);

    has key         => (is => 'ro', isa => Int, required => 1);

    has midi_ch     => (is => 'ro', isa => Int, default => sub { 1 });
    has buffer_size => (is => 'ro', isa => Int, default => sub { 554 });
    has division    => (is => 'ro', isa => Int, default => sub { 96 });
    has sample_rate => (is => 'ro', isa => Int, default => sub { 46536 });
    has tempo       => (is => 'ro', isa => Int, default => sub { 500_000 });

    has events      => (is => 'ro', isa => ArrayRef[Object], default => sub { [] });
    has messages    => (is => 'rw', isa => ArrayRef[ArrayRef]);

    has count       => (is => 'rw', isa => Int, default => sub { 0 });
    has last        => (is => 'rw', isa => Num, default => sub { 0 });
    has start       => (is => 'rw', isa => Int);
    has sum         => (is => 'rw', isa => Num, default => sub { 0 });
    has total       => (is => 'rw', isa => Int, default => sub { 0 });

    sub process($self, $sample) {
        $sample = sqrt($sample);
        if (!$self->last && $sample) {
            $self->start($self->total);
            $self->sum($sample);
            $self->count(1);
        } elsif ($sample) {
            $self->sum($self->sum + $sample);
            $self->count($self->count + 1);
        } elsif ($self->last && !$sample) {
            $self->generate_event;
        }

        $self->last($sample);
        $self->total($self->total + 1);
        return;
    }

    sub finalize($self) {
        $self->generate_event if $self->count;

        sub round($n) { return 0 + sprintf('%.0f', $n) }

        my $pcm_factor = $self->buffer_size / $self->sample_rate;
        my $midi_factor = $self->tempo / $self->division / 1_000_000;

        my @messages;
        for my $event ($self->events->@*) {
            push @messages => [
                $self->midi_ch,
                round(($pcm_factor * $event->start) / $midi_factor),
                'Note_on_c',
                0,
                21 + $self->key,
                round($event->velocity * 127),
            ];
            push @messages => [
                $self->midi_ch,
                round(($pcm_factor * $event->finish) / $midi_factor),
                'Note_off_c',
                0,
                21 + $self->key,
                64,
            ];
        }
        $self->messages([sort { $a->[1] <=> $b->[1] } @messages]);

        return;
    }

    sub generate_event($self) {
        push $self->events->@* => NoteEvent->new( 
            start       => $self->start,
            finish      => $self->total,
            velocity    => $self->sum / $self->count,
        );
        $self->count(0);
        return;
    }
}

package main {
    use Data::Printer;

    sub main(@argv) {
        my $KEYS = 88;

        my @detectors = map { TransientDetector->new(key => $_) } 0 .. ($KEYS - 1);
        while (my $line = <>) {
            chomp $line;
            my @levels = map { $_ / 255 } unpack 'C*' => pack 'H*' => $line;
            die "WTF\n" if $KEYS != scalar @levels;

            $detectors[$_]->process($levels[$_]) for 0 .. ($KEYS - 1);
        }
        $_->finalize for @detectors;

        p $_->messages for @detectors;

        return 0;
    }

    exit main(@ARGV);
}