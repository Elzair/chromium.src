// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_QUIC_CRYPTO_CLIENT_CONFIG_H_
#define NET_QUIC_CRYPTO_QUIC_CRYPTO_CLIENT_CONFIG_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_protocol.h"

namespace net {

class ChannelIDSigner;
class CryptoHandshakeMessage;
class ProofVerifier;
class ProofVerifyDetails;
class QuicRandom;
class QuicSessionKey;

// QuicCryptoClientConfig contains crypto-related configuration settings for a
// client. Note that this object isn't thread-safe. It's designed to be used on
// a single thread at a time.
class NET_EXPORT_PRIVATE QuicCryptoClientConfig : public QuicCryptoConfig {
 public:
  // A CachedState contains the information that the client needs in order to
  // perform a 0-RTT handshake with a server. This information can be reused
  // over several connections to the same server.
  class NET_EXPORT_PRIVATE CachedState {
   public:
    CachedState();
    ~CachedState();

    // IsComplete returns true if this object contains enough information to
    // perform a handshake with the server. |now| is used to judge whether any
    // cached server config has expired.
    bool IsComplete(QuicWallTime now) const;

    // IsEmpty returns true if |server_config_| is empty.
    bool IsEmpty() const;

    // GetServerConfig returns the parsed contents of |server_config|, or NULL
    // if |server_config| is empty. The return value is owned by this object
    // and is destroyed when this object is.
    const CryptoHandshakeMessage* GetServerConfig() const;

    // SetServerConfig checks that |server_config| parses correctly and stores
    // it in |server_config_|. |now| is used to judge whether |server_config|
    // has expired.
    QuicErrorCode SetServerConfig(base::StringPiece server_config,
                                  QuicWallTime now,
                                  std::string* error_details);

    // InvalidateServerConfig clears the cached server config (if any).
    void InvalidateServerConfig();

    // SetProof stores a certificate chain and signature.
    void SetProof(const std::vector<std::string>& certs,
                  base::StringPiece signature);

    // Clears the certificate chain and signature and invalidates the proof.
    void ClearProof();

    // SetProofValid records that the certificate chain and signature have been
    // validated and that it's safe to assume that the server is legitimate.
    // (Note: this does not check the chain or signature.)
    void SetProofValid();

    // If the server config or the proof has changed then it needs to be
    // revalidated. Helper function to keep server_config_valid_ and
    // generation_counter_ in sync.
    void SetProofInvalid();

    const std::string& server_config() const;
    const std::string& source_address_token() const;
    const std::vector<std::string>& certs() const;
    const std::string& signature() const;
    bool proof_valid() const;
    uint64 generation_counter() const;
    const ProofVerifyDetails* proof_verify_details() const;

    void set_source_address_token(base::StringPiece token);

    // SetProofVerifyDetails takes ownership of |details|.
    void SetProofVerifyDetails(ProofVerifyDetails* details);

    // Copy the |server_config_|, |source_address_token_|, |certs_| and
    // |server_config_sig_| from the |other|.  The remaining fields,
    // |generation_counter_|, |proof_verify_details_|, and |scfg_| remain
    // unchanged.
    void InitializeFrom(const CachedState& other);

    // Initializes this cached state based on the arguments provided.
    // Returns false if there is a problem parsing the server config.
    bool Initialize(base::StringPiece server_config,
                    base::StringPiece source_address_token,
                    const std::vector<std::string>& certs,
                    base::StringPiece signature,
                    QuicWallTime now);

   private:
    std::string server_config_;         // A serialized handshake message.
    std::string source_address_token_;  // An opaque proof of IP ownership.
    std::vector<std::string> certs_;    // A list of certificates in leaf-first
                                        // order.
    std::string server_config_sig_;     // A signature of |server_config_|.
    bool server_config_valid_;          // True if |server_config_| is correctly
                                        // signed and |certs_| has been
                                        // validated.
    // Generation counter associated with the |server_config_|, |certs_| and
    // |server_config_sig_| combination. It is incremented whenever we set
    // server_config_valid_ to false.
    uint64 generation_counter_;

    scoped_ptr<ProofVerifyDetails> proof_verify_details_;

    // scfg contains the cached, parsed value of |server_config|.
    mutable scoped_ptr<CryptoHandshakeMessage> scfg_;

    DISALLOW_COPY_AND_ASSIGN(CachedState);
  };

  QuicCryptoClientConfig();
  ~QuicCryptoClientConfig();

  // Sets the members to reasonable, default values.
  void SetDefaults();

  // LookupOrCreate returns a CachedState for the given |server_key|. If no such
  // CachedState currently exists, it will be created and cached.
  CachedState* LookupOrCreate(const QuicSessionKey& server_key);

  // FillInchoateClientHello sets |out| to be a CHLO message that elicits a
  // source-address token or SCFG from a server. If |cached| is non-NULL, the
  // source-address token will be taken from it. |out_params| is used in order
  // to store the cached certs that were sent as hints to the server in
  // |out_params->cached_certs|. |preferred_version| is the version of the
  // QUIC protocol that this client chose to use initially. This allows the
  // server to detect downgrade attacks.
  void FillInchoateClientHello(const QuicSessionKey& server_key,
                               const QuicVersion preferred_version,
                               const CachedState* cached,
                               QuicCryptoNegotiatedParameters* out_params,
                               CryptoHandshakeMessage* out) const;

  // FillClientHello sets |out| to be a CHLO message based on the configuration
  // of this object. This object must have cached enough information about
  // the server's hostname in order to perform a handshake. This can be checked
  // with the |IsComplete| member of |CachedState|.
  //
  // |clock| and |rand| are used to generate the nonce and |out_params| is
  // filled with the results of the handshake that the server is expected to
  // accept. |preferred_version| is the version of the QUIC protocol that this
  // client chose to use initially. This allows the server to detect downgrade
  // attacks.
  QuicErrorCode FillClientHello(const QuicSessionKey& server_key,
                                QuicConnectionId connection_id,
                                const QuicVersion preferred_version,
                                const CachedState* cached,
                                QuicWallTime now,
                                QuicRandom* rand,
                                QuicCryptoNegotiatedParameters* out_params,
                                CryptoHandshakeMessage* out,
                                std::string* error_details) const;

  // ProcessRejection processes a REJ message from a server and updates the
  // cached information about that server. After this, |IsComplete| may return
  // true for that server's CachedState. If the rejection message contains
  // state about a future handshake (i.e. an nonce value from the server), then
  // it will be saved in |out_params|. |now| is used to judge whether the
  // server config in the rejection message has expired.
  QuicErrorCode ProcessRejection(const CryptoHandshakeMessage& rej,
                                 QuicWallTime now,
                                 CachedState* cached,
                                 QuicCryptoNegotiatedParameters* out_params,
                                 std::string* error_details);

  // ProcessServerHello processes the message in |server_hello|, updates the
  // cached information about that server, writes the negotiated parameters to
  // |out_params| and returns QUIC_NO_ERROR. If |server_hello| is unacceptable
  // then it puts an error message in |error_details| and returns an error
  // code. |negotiated_versions| contains the list of version, if any, that were
  // present in a version negotiation packet previously recevied from the
  // server. The contents of this list will be compared against the list of
  // versions provided in the VER tag of the server hello.
  QuicErrorCode ProcessServerHello(const CryptoHandshakeMessage& server_hello,
                                   QuicConnectionId connection_id,
                                   const QuicVersionVector& negotiated_versions,
                                   CachedState* cached,
                                   QuicCryptoNegotiatedParameters* out_params,
                                   std::string* error_details);

  ProofVerifier* proof_verifier() const;

  // SetProofVerifier takes ownership of a |ProofVerifier| that clients are
  // free to use in order to verify certificate chains from servers. If a
  // ProofVerifier is set then the client will request a certificate chain from
  // the server.
  void SetProofVerifier(ProofVerifier* verifier);

  ChannelIDSigner* channel_id_signer() const;

  // SetChannelIDSigner sets a ChannelIDSigner that will be called when the
  // server supports channel IDs to sign a message proving possession of the
  // given ChannelID. This object takes ownership of |signer|.
  void SetChannelIDSigner(ChannelIDSigner* signer);

  // Initialize the CachedState from |canonical_crypto_config| for the
  // |canonical_server_key| as the initial CachedState for |server_key|. We will
  // copy config data only if |canonical_crypto_config| has valid proof.
  void InitializeFrom(const QuicSessionKey& server_key,
                      const QuicSessionKey& canonical_server_key,
                      QuicCryptoClientConfig* canonical_crypto_config);

  // Adds |suffix| as a domain suffix for which the server's crypto config
  // is expected to be shared among servers with the domain suffix. If a server
  // matches this suffix, then the server config from another server with the
  // suffix will be used to initialize the cached state for this server.
  void AddCanonicalSuffix(const std::string& suffix);

  // Prefers AES-GCM (kAESG) over other AEAD algorithms. Call this method if
  // the CPU has hardware acceleration for AES-GCM. This method can only be
  // called after SetDefaults().
  void PreferAesGcm();

  // Disables the use of ECDSA for proof verification.
  // Call this method on platforms that do not support ECDSA.
  // TODO(rch): remove this method when we drop support for Windows XP.
  void DisableEcdsa();

 private:
  typedef std::map<QuicSessionKey, CachedState*> CachedStateMap;

  // If the suffix of the hostname in |server_key| is in |canoncial_suffixes_|,
  // then populate |cached| with the canonical cached state from
  // |canonical_server_map_| for that suffix.
  void PopulateFromCanonicalConfig(const QuicSessionKey& server_key,
                                   CachedState* cached);

  // cached_states_ maps from the server_key to the cached information about
  // that server.
  CachedStateMap cached_states_;

  // Contains a map of servers which could share the same server config. Map
  // from a canonical host suffix/port/scheme to a representative server with
  // the canonical suffix, which has a plausible set of initial certificates
  // (or at least server public key).
  std::map<QuicSessionKey, QuicSessionKey> canonical_server_map_;

  // Contains list of suffixes (for exmaple ".c.youtube.com",
  // ".googlevideo.com") of canoncial hostnames.
  std::vector<std::string> canoncial_suffixes_;

  scoped_ptr<ProofVerifier> proof_verifier_;
  scoped_ptr<ChannelIDSigner> channel_id_signer_;

  // True if ECDSA should be disabled.
  bool disable_ecdsa_;

  DISALLOW_COPY_AND_ASSIGN(QuicCryptoClientConfig);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_QUIC_CRYPTO_CLIENT_CONFIG_H_
