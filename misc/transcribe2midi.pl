#!/usr/bin/env perl
use 5.036;

package Configuration {
    use Moo;
    use MooX::Options;

    use File::Basename qw(basename);
    use File::Spec ();
    use FindBin qw($RealBin);

    option buffer_size  => (is => 'ro', format => 'i', default => sub { 554 });
    option channel      => (is => 'ro', format => 'i', default => sub { 0 });
    option division     => (is => 'ro', format => 'i', default => sub { 960 });
    option ffmpeg       => (is => 'ro', format => 's', default => sub { 'ffmpeg' });
    option filters      => (is => 'ro', format => 's', default => sub { 'asubcut=27,asupercut=20000' });
    option input        => (is => 'ro', format => 's', default => sub { 'audio/chromatic.mp3' });
    option keys         => (is => 'ro', format => 'i', default => sub { 88 });
    option min_length   => (is => 'ro', format => 'f', default => sub { 0.1 });
    option normalize    => (is => 'ro', negatable => 1, default => sub { 1 });
    option output       => (is => 'ro', format => 's', builder => 1);
    option overwrite    => (is => 'ro');
    option pianolizer   => (is => 'ro', format => 's', builder => 1);
    option pitchfork    => (is => 'ro', format => 'f', default => sub { 440.0 });
    option reference    => (is => 'ro', format => 'i', default => sub { 48 });
    option sample_rate  => (is => 'ro', format => 'i', default => sub { 46536 });
    option smoothing    => (is => 'ro', format => 'f', default => sub { 0.04 });
    option tempo        => (is => 'ro', format => 'i', default => sub { 500_000 });
    option threshold    => (is => 'ro', format => 'f', default => sub { 0.05 });
    option tolerance    => (is => 'ro', format => 'f', default => sub { 1.0 });

    sub _build_output($self) { basename($self->input) =~ s{ \. \w+ $ }{.mid}rx }
    sub _build_pianolizer($self) { File::Spec->catfile($RealBin, '..', 'pianolizer') }

    has K => (is => 'lazy');
    sub _build_K($self) { $self->keys - 1 }

    sub BUILD($self, $args) {
        die "'@{[ $self->input ]}' is not a file!\n\n"
            unless -f $self->input;
        die "'@{[ $self->output ]}' already exists! Hint: --overwrite\n\n"
            if !$self->overwrite && -e $self->output;
        die "'@{[ $self->pianolizer ]}' is not an executable!\n\n"
            unless -x $self->pianolizer;
    }
}

package MIDIEvent {
    # http://www.music.mcgill.ca/~ich/classes/mumt306/StandardMIDIfileformat.html
    use Moo;

    use POSIX qw(round);
    use Types::Standard qw(Bool Int Num);

    has channel     => (is => 'ro', isa => Int, required => 1);
    has factor      => (is => 'ro', isa => Num, required => 1);
    has key         => (is => 'ro', isa => Int, required => 1);
    has state       => (is => 'ro', isa => Bool, required => 1);
    has time        => (is => 'ro', isa => Int, required => 1);
    has velocity    => (is => 'ro', isa => Num, required => 1);

    has status      => (is => 'lazy', isa => Int);
    sub _build_status($self) { ((0x8 + $self->state) << 4) | $self->channel }

    sub serialize($self, $max_velocity) {
        state $last_time = 0;
        state $last_status = 0;

        my $time = round($self->factor * $self->time);
        my $delta = $time - $last_time;
        $last_time = $time;

        my @status = $self->status == $last_status ? () : ($self->status);
        $last_status = $self->status;

        return pack(
            'wC*',
            $delta,
            @status,
            21 + $self->key,
            round(($self->velocity / $max_velocity) * 127),
        );
    }
}

package TransientDetector {
    use Moo;
    use Types::Standard qw(ArrayRef HashRef InstanceOf Int Num);

    has config      => (
        is          => 'ro',
        isa         => InstanceOf['Configuration'],
        handles     => [qw[buffer_size channel division min_length sample_rate tempo]],
        required    => 1,
    );
    has key         => (is => 'ro', isa => Int, required => 1);

    has count       => (is => 'rw', isa => Int, default => sub { 0 });
    has last        => (is => 'rw', isa => Num, default => sub { 0 });
    has start       => (is => 'rw', isa => Int);
    has sum         => (is => 'rw', isa => Num, default => sub { 0 });
    has total       => (is => 'rw', isa => Int, default => sub { 0 });

    has events      => (is => 'ro', isa => ArrayRef, default => sub { [] });

    has factor      => (is => 'lazy', isa => Num);
    sub _build_factor($self) { $self->pcm_factor / $self->midi_factor }

    has pcm_factor  => (is => 'lazy', isa => Num);
    sub _build_pcm_factor($self) { $self->buffer_size / $self->sample_rate }

    has midi_factor => (is => 'lazy', isa => Num);
    sub _build_midi_factor($self) { $self->tempo / $self->division / 1_000_000 }

    has common_opts => (is => 'lazy', isa => HashRef);
    sub _build_common_opts($self) {
        return {
            channel     => $self->channel,
            key         => $self->key,
            factor      => $self->factor,
        };
    }

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
            $self->_generate_events;
        }
        $self->last($sample);
        $self->total($self->total + 1);
        return;
    }

    sub finalize($self) {
        $self->_generate_events if $self->count;
        return;
    }

    sub _generate_events($self) {
        my $velocity = $self->sum / $self->count;
        $self->count(0);

        my $delta = $self->pcm_factor * ($self->total - $self->start);
        return if $delta < $self->min_length;

        push $self->events->@* => MIDIEvent->new(
            $self->common_opts->%*,
            state       => 1,
            time        => $self->start,
            velocity    => $velocity,
        );
        push $self->events->@* => MIDIEvent->new(
            $self->common_opts->%*,
            state       => 0,
            time        => $self->total,
            velocity    => 0.5,
        );
        return;
    }
}

package Transcriber {
    use Moo;
    use List::Util qw(max);
    use Types::Standard qw(ArrayRef InstanceOf);
    use Term::ProgressBar ();

    has config      => (
        is          => 'ro',
        isa         => InstanceOf['Configuration'],
        handles     => [qw[K buffer_size division keys normalize sample_rate]],
        required    => 1,
    );

    has detectors   => (is => 'lazy', isa => ArrayRef);
    sub _build_detectors($self) {
        return [map {
            TransientDetector->new(
                config      => $self->config,
                key         => $_,
            )
        } 0 .. $self->K];
    }

    sub header_chunk($division) { pack('NNnnn', 0x4D546864, 6, 0, 1, $division) }
    sub track_chunk($track_length) { pack('NN', 0x4D54726B, $track_length) }
    sub end_of_track { pack('wC2w', 0, 0xFF, 0x2F, 0) }

    sub process($self, $buffer) {
        my @buffer = split m{\n}x, $buffer;

        my $progress = Term::ProgressBar->new({
            count   => $#buffer,
            name    => 'Process',
            remove  => 1,
        });

        my $n = 0;
        my $step = int($self->sample_rate / $self->buffer_size);
        for my $line (@buffer) {
            my @levels = map { $_ / 255 } unpack 'C*' => pack 'H*' => $line;
            die "WTF\n" if $self->K != $#levels;

            $self->detectors->[$_]->process($levels[$_]) for 0 .. $self->K;

            $progress->update($n) if ++$n % $step == 0;
        }
        $progress->update($#buffer);
        $_->finalize for $self->detectors->@*;

        my @midi = $self->generate_midi;
        die "no music detected\n" unless @midi;

        printf STDERR "%d MIDI events extracted\n", scalar(@midi) / 2;

        my $track = join '', @midi, end_of_track;
        my $output = header_chunk($self->division);
        $output .= track_chunk(length $track);
        $output .= $track;

        return $output;
    }

    sub generate_midi($self) {
        my @all_events = map { $_->events->@* } $self->detectors->@*;

        my $max_velocity = $self->normalize
            ? max map { $_->velocity } @all_events
            : 1.0;

        return map {
            $_->serialize($max_velocity)
        } sort {
            ($a->time <=> $b->time) || ($a->key <=> $b->key)
        } @all_events;
    }
}

package main {
    use Fcntl qw(O_CREAT O_WRONLY);
    use IPC::Run qw(run);
    use Term::ProgressBar ();

    sub main() {
        my $config = Configuration->new_with_options;

        my @ffmpeg = (
            $config->ffmpeg,
            '-loglevel' => 'info',
            '-i'        => $config->input,
            '-ac'       => 1,
            '-af'       => $config->filters,
            '-ar'       => $config->sample_rate,
            '-f'        => 'f32le',
            '-c:a'      => 'pcm_f32le',
            '-',
        );
        my @pianolizer = (
            $config->pianolizer,
            '-a'        => $config->smoothing,
            '-b'        => $config->buffer_size,
            '-k'        => $config->keys,
            '-p'        => $config->pitchfork,
            '-r'        => $config->reference,
            '-s'        => $config->sample_rate,
            '-t'        => $config->threshold,
            '-x'        => $config->tolerance,
        );
        run \@ffmpeg => '|' => \@pianolizer => \my $buffer,
            '2>' => \&ffmpeg_progress;

        my $transcriber = Transcriber->new(config => $config);
        $buffer = $transcriber->process($buffer);

        my $out = \*STDOUT;
        if ($config->output ne '-') {
            unlink $config->output;
            sysopen($out, $config->output, O_CREAT | O_WRONLY)
                || die "can't write to '@{[ $config->output ]}'\n\n";
        }
        syswrite($out, $buffer);

        return 0;
    }

    sub to_seconds { return $1 * 3600 + $2 * 60 + $3 + $4 / 100 }

    sub ffmpeg_progress ($line) {
        state $progress;
        my $timestamp = qr{ (\d{2,}) : (\d{2}) : (\d{2}) \. (\d{2}) }x;
        if ($line =~ m{ \b Duration: \s+ $timestamp \b }x) {
            $progress = Term::ProgressBar->new({
                count   => to_seconds,
                name    => 'Preprocess',
                remove  => 1,
            });
        } elsif ($progress && $line =~ m{ \b time = $timestamp \b }x) {
            $progress->update(to_seconds);
        }
        return;
    }

    exit main();
}
