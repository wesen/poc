require 'dl/import'

module POC
    module FEC
        extend DL::Importable
        dlload "./libfec.so"

        # misc
        # FIXME: hier korrekter type und rausfinden 
        # wie NULL Pointer in libfec_init uebergeben werden 
        extern "void libfec_init(void *, void *) "
        extern "void libfec_close()"

        # IO
        extern "int libfec_read_adu(unsigned char *, unsigned int)"
        extern "void libfec_write_adu(unsigned char *, unsigned int)"

        # decoding
        extern "fec_decode_t *libfec_new_group(unsigned char,
                                               unsigned char,
                                               unsigned long)"

        extern "void libfec_add_pkt(fec_decode_t *,
                                    unsigned char,
                                    unsigned long,
                                    unsigned char*)"

        extern "unsigned int libfec_decode(fec_decode_t *,
                                           unsigned char *,
                                           unsigned int,
                                           unsigned int)"

        extern "void libfec_delete_group(fec_decode_t *)"

        #encoding
        extern "fec_encode_t *libfec_new_encode(unsigned char,
                                                unsigned char)"

        extern "int libfec_add_adu(fec_encode_t *,
                                   unsigned long,
                                   unsigned char *)"

        extern "unsigned int libfec_max_length(fec_encode_t *)"

        extern "unsigned int libfec_encode(fec_encode_t *,
                                           unsigned char *,
                                           unsigned int,
                                           unsigned int)"
                                                
        extern "void libfec_delete_encode(fec_encode_t *)"

        # init library
        libfec_init(nil, nil)

        class Error < Exception 
        end

        class Encoder
            def initialize(n, k)
                @group = FEC::libfec_new_encode(n, k)
                raise Error.new("cannot allocate new FEC group") unless @group
                if block_given?
                    begin
                        yield self
                    ensure
                        close
                    end
                end
            end

            def add(data)
                success = FEC::libfec_add_adu(@group, data.size, data) == 1
                raise Error.new("cannot add ADU to FEC group") unless success
            end

            def maxlen
                FEC::libfec_max_length(@group)
            end

            def encode(idx)
                bufferlen = maxlen
                buffer    = (" " * bufferlen).to_ptr
                encoded   = FEC::libfec_encode(@group, buffer, idx, bufferlen)
                buffer.to_s(encoded)
            end

            def close
                FEC::libfec_delete_encode(@group)
            end
        end

        class Decoder
            def initialize(n, k, len)
                @group  = FEC::libfec_new_group(n, k, len)
                @maxlen = len
                raise Error.new("cannot allocate new FEC group") unless @group
                if block_given?
                    begin
                        yield self
                    ensure
                        close
                    end
                end
            end

            def add(seq, data)
                FEC::libfec_add_pkt(@group, seq, data.size, data)
            end

            def decode(idx)
                buffer  = (" " * @maxlen).to_ptr
                decoded = FEC::libfec_decode(@group, buffer, idx, @maxlen) 
                return nil if decoded == 0
                buffer.to_s(decoded)
            end

            def close
                FEC::libfec_delete_group(@group)
            end
        end

        def encode(pkts, k)
            raise Error.new("#pkts (#{pkts.size}) > k (#{k})") if pkts.size > k
            res    = []
            maxlen = 0 
            Encoder.new(pkts.size, k) do |encoder|
                pkts.each do |pkt|
                    encoder.add(pkt)
                end
                k.times do |idx|
                    res << encoder.encode(idx)
                end
                maxlen = encoder.maxlen
            end
            [maxlen, res]
        end

        # ... [pkt, nil, pkt, pkt, ...]
        def decode(pkts, n)
            maxlen = pkts.map{ |pkt| pkt ? pkt.size : 0 }.max
            res = []
            Decoder.new(n, pkts.size, maxlen) do |decoder|
                pkts.each_with_index do |pkt, idx|
                    next unless pkt
                    decoder.add(idx, pkt)
                end
                n.times do |idx|
                    res << decoder.decode(idx)
                end
            end
            res
        end

        module_function :decode, :encode
    end
end

if __FILE__ == $0

    # 3 Datenpakete ... 
    data = ["foooo", "bl0rg", "baaaz"]

    p data

    # ... aufblasen auf 5 (n = 3, k = 5)
    maxlen, encoded = POC::FEC::encode(data, 5)

    p encoded

    # Paket 1 und 2 gehen vorloren
    encoded[1] = nil
    encoded[2] = nil

    # die 3 original Pakete zurueckholen
    decoded = POC::FEC::decode(encoded, 3)

    p decoded
end
