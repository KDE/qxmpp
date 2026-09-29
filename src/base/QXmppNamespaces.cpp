// SPDX-FileCopyrightText: 2026 Linus Jahn <lnj@kaidan.im>
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QXmppNamespaces.h"

#include "QXmppConstants_p.h"

#include "Enums.h"

using namespace QXmpp;
using namespace QXmpp::Private;

template<>
struct Enums::Data<Namespace> {
    static constexpr auto Values = makeValues<Namespace>({
        { Namespace::Xml, ns_xml },
        { Namespace::Pie0, ns_pie },
        { Namespace::Stream, ns_stream },
        { Namespace::Client, ns_client },
        { Namespace::Server, ns_server },
        { Namespace::Roster, ns_roster },
        { Namespace::Tls, ns_tls },
        { Namespace::Sasl, ns_sasl },
        { Namespace::Bind, ns_bind },
        { Namespace::StreamError, ns_stream_error },
        { Namespace::Session, ns_session },
        { Namespace::Stanza, ns_stanza },
        { Namespace::PreApproval, ns_pre_approval },
        { Namespace::RosterVersioning, ns_rosterver },
        { Namespace::Rpc, ns_rpc },
        { Namespace::FeatureNegotiation, ns_feature_negotiation },
        { Namespace::LegacyOpenPgp, ns_legacy_openpgp },
        { Namespace::DiscoInfo, ns_disco_info },
        { Namespace::DiscoItems, ns_disco_items },
        { Namespace::ExtendedAddressing, ns_extended_addressing },
        { Namespace::Muc, ns_muc },
        { Namespace::MucAdmin, ns_muc_admin },
        { Namespace::MucOwner, ns_muc_owner },
        { Namespace::MucUser, ns_muc_user },
        { Namespace::MucRoomInfo, ns_muc_roominfo },
        { Namespace::MucRequest, ns_muc_request },
        { Namespace::MucRoomConfig, ns_muc_roomconfig },
        { Namespace::Ibb, ns_ibb },
        { Namespace::Bookmarks, ns_bookmarks },
        { Namespace::PrivateXmlStorage, ns_private },
        { Namespace::VCard, ns_vcard },
        { Namespace::Rsm, ns_rsm },
        { Namespace::PubSub, ns_pubsub },
        { Namespace::PubSubAutoCreate, ns_pubsub_auto_create },
        { Namespace::PubSubConfigNode, ns_pubsub_config_node },
        { Namespace::PubSubConfigNodeMax, ns_pubsub_config_node_max },
        { Namespace::PubSubCreateAndConfigure, ns_pubsub_create_and_configure },
        { Namespace::PubSubCreateNodes, ns_pubsub_create_nodes },
        { Namespace::PubSubErrors, ns_pubsub_errors },
        { Namespace::PubSubEvent, ns_pubsub_event },
        { Namespace::PubSubMultiItems, ns_pubsub_multi_items },
        { Namespace::PubSubNodeConfig, ns_pubsub_node_config },
        { Namespace::PubSubOwner, ns_pubsub_owner },
        { Namespace::PubSubPublish, ns_pubsub_publish },
        { Namespace::PubSubPublishOptions, ns_pubsub_publish_options },
        { Namespace::PubSubRsm, ns_pubsub_rsm },
        { Namespace::Bytestreams, ns_bytestreams },
        { Namespace::OutOfBandData, ns_oob },
        { Namespace::Xhtml, ns_xhtml },
        { Namespace::XhtmlIm, ns_xhtml_im },
        { Namespace::Register, ns_register },
        { Namespace::RegisterFeature, ns_register_feature },
        { Namespace::Auth, ns_auth },
        { Namespace::AuthFeature, ns_authFeature },
        { Namespace::Geoloc, ns_geoloc },
        { Namespace::GeolocNotify, ns_geoloc_notify },
        { Namespace::UserAvatarData, ns_user_avatar_data },
        { Namespace::UserAvatarMetadata, ns_user_avatar_metadata },
        { Namespace::ChatStates, ns_chat_states },
        { Namespace::LegacyDelayedDelivery, ns_legacy_delayed_delivery },
        { Namespace::SoftwareVersion, ns_version },
        { Namespace::DataForms, ns_data },
        { Namespace::StreamInitiation, ns_stream_initiation },
        { Namespace::StreamInitiationFileTransfer, ns_stream_initiation_file_transfer },
        { Namespace::UrlData, ns_url_data },
        { Namespace::Activity, ns_activity },
        { Namespace::Capabilities, ns_capabilities },
        { Namespace::Tune, ns_tune },
        { Namespace::TuneNotify, ns_tune_notify },
        { Namespace::Archive, ns_archive },
        { Namespace::Compress, ns_compress },
        { Namespace::CompressFeature, ns_compressFeature },
        { Namespace::RosterNotes, ns_rosternotes },
        { Namespace::VCardUpdate, ns_vcard_update },
        { Namespace::ContactAddresses, ns_contact_addresses },
        { Namespace::Captcha, ns_captcha },
        { Namespace::Jingle1, ns_jingle },
        { Namespace::JingleRawUdp1, ns_jingle_raw_udp },
        { Namespace::JingleIceUdp1, ns_jingle_ice_udp },
        { Namespace::JingleErrors1, ns_jingle_errors },
        { Namespace::JingleRtp1, ns_jingle_rtp },
        { Namespace::JingleRtpAudio, ns_jingle_rtp_audio },
        { Namespace::JingleRtpVideo, ns_jingle_rtp_video },
        { Namespace::JingleRtpInfo1, ns_jingle_rtp_info },
        { Namespace::JingleRtpErrors1, ns_jingle_rtp_errors },
        { Namespace::MessageReceipts, ns_message_receipts },
        { Namespace::Blocking, ns_blocking },
        { Namespace::StreamManagement3, ns_stream_management },
        { Namespace::Ping, ns_ping },
        { Namespace::EntityTime, ns_entity_time },
        { Namespace::DelayedDelivery, ns_delayed_delivery },
        { Namespace::ExternalServiceDiscovery2, ns_external_service_discovery },
        { Namespace::ServerDialback, ns_server_dialback },
        { Namespace::MediaElement, ns_media_element },
        { Namespace::Attention0, ns_attention },
        { Namespace::BitsOfBinary, ns_bob },
        { Namespace::DirectMucInvitation, ns_conference },
        { Namespace::Thumbnails1, ns_thumbs },
        { Namespace::Muji0, ns_muji },
        { Namespace::Carbons2, ns_carbons },
        { Namespace::Moved1, ns_moved },
        { Namespace::JingleRtcpFeedback0, ns_jingle_rtcp_fb },
        { Namespace::JingleRtpHeaderExtensions0, ns_jingle_rtp_hdrext },
        { Namespace::Forwarding0, ns_forwarding },
        { Namespace::Hashes2, ns_hashes },
        { Namespace::MucUnique, ns_muc_unique },
        { Namespace::MessageCorrection0, ns_message_correct },
        { Namespace::Mam2, ns_mam },
        { Namespace::Idle1, ns_idle },
        { Namespace::JingleDtls0, ns_jingle_dtls },
        { Namespace::ChatMarkers0, ns_chat_markers },
        { Namespace::MessageProcessingHints, ns_message_processing_hints },
        { Namespace::Csi0, ns_csi },
        { Namespace::JingleMessage0, ns_jingle_message },
        { Namespace::Push0, ns_push },
        { Namespace::StanzaId0, ns_sid },
        { Namespace::HttpUpload0, ns_http_upload },
        { Namespace::Otr0, ns_otr },
        { Namespace::MessageAttaching1, ns_message_attaching },
        { Namespace::MixCore1, ns_mix },
        { Namespace::MixCore1CreateChannel, ns_mix_create_channel },
        { Namespace::MixCore1Searchable, ns_mix_searchable },
        { Namespace::MixNodesInfo, ns_mix_node_info },
        { Namespace::MixNodesMessages, ns_mix_node_messages },
        { Namespace::MixNodesParticipants, ns_mix_node_participants },
        { Namespace::OpenPgp0, ns_ox },
        { Namespace::Reporting1, ns_reporting },
        { Namespace::Eme0, ns_eme },
        { Namespace::Spoiler0, ns_spoiler },
        { Namespace::LegacyOmemo, ns_omemo },
        { Namespace::Omemo1, ns_omemo_1 },
        { Namespace::Omemo2, ns_omemo_2 },
        { Namespace::Omemo2Bundles, ns_omemo_2_bundles },
        { Namespace::Omemo2Devices, ns_omemo_2_devices },
        { Namespace::Bind2_0, ns_bind2 },
        { Namespace::Sasl2_2, ns_sasl_2 },
        { Namespace::Bookmarks2_1, ns_bookmarks2 },
        { Namespace::MixNodesPresence, ns_mix_node_presence },
        { Namespace::MixNodesJidMap, ns_mix_node_jidmap },
        { Namespace::MixPam2, ns_mix_pam },
        { Namespace::MixPam2Archive, ns_mix_pam_archiving },
        { Namespace::MixRoster0, ns_mix_roster },
        { Namespace::MixPresence0, ns_mix_presence },
        { Namespace::MixAdmin0, ns_mix_admin },
        { Namespace::MixNodesAllowed, ns_mix_node_allowed },
        { Namespace::MixNodesBanned, ns_mix_node_banned },
        { Namespace::MixNodesConfig, ns_mix_node_config },
        { Namespace::MixMisc0, ns_mix_misc },
        { Namespace::MucSelfPingOptimization, ns_muc_self_ping },
        { Namespace::OccupantId0, ns_muc_occupant_id },
        { Namespace::MessageRetract1, ns_message_retract },
        { Namespace::MessageRetract1Tombstone, ns_message_retract_tombstone },
        { Namespace::MessageModerate1, ns_message_moderate },
        { Namespace::Fallback0, ns_fallback_indication },
        { Namespace::TrustMessages1, ns_tm },
        { Namespace::Reactions0, ns_reactions },
        { Namespace::FileMetadata0, ns_file_metadata },
        { Namespace::StatelessFileSharing0, ns_sfs },
        { Namespace::EncryptedStatelessFileSharing0, ns_esfs },
        { Namespace::Atm1, ns_atm },
        { Namespace::Reply0, ns_reply },
        { Namespace::CallInvites0, ns_call_invites },
        { Namespace::Fast0, ns_fast },
    });
};

/*!
    \enum QXmpp::Namespace
    \inmodule QXmpp

    XML namespaces known to QXmpp.

    The names consist of the name the specification is known by and the version from the
    URI, e.g. Mam2 for \c{urn:xmpp:mam:2}. If the name ends with a digit, an underscore
    separates the version, e.g. Bind2_0 for \c{urn:xmpp:bind:0} of Bind 2. Namespaces
    without a version in their URI have no suffix.

    Use namespaceUri() and namespaceFromUri() to convert between the values and the URIs.

    \value Xml XML: \c{http://www.w3.org/XML/1998/namespace}
    \value Pie0 \xep{0227}{Portable Import/Export Format for XMPP-IM Servers}: \c{urn:xmpp:pie:0}
    \value Stream XMPP: \c{http://etherx.jabber.org/streams}
    \value Client XMPP: \c{jabber:client}
    \value Server XMPP: \c{jabber:server}
    \value Roster XMPP: \c{jabber:iq:roster}
    \value Tls XMPP: \c{urn:ietf:params:xml:ns:xmpp-tls}
    \value Sasl XMPP: \c{urn:ietf:params:xml:ns:xmpp-sasl}
    \value Bind XMPP: \c{urn:ietf:params:xml:ns:xmpp-bind}
    \value StreamError XMPP: \c{urn:ietf:params:xml:ns:xmpp-streams}
    \value Session XMPP: \c{urn:ietf:params:xml:ns:xmpp-session}
    \value Stanza XMPP: \c{urn:ietf:params:xml:ns:xmpp-stanzas}
    \value PreApproval XMPP: \c{urn:xmpp:features:pre-approval}
    \value RosterVersioning XMPP: \c{urn:xmpp:features:rosterver}
    \value Rpc \xep{0009}{Jabber-RPC}: \c{jabber:iq:rpc}
    \value FeatureNegotiation \xep{0020}{Feature Negotiation}: \c{http://jabber.org/protocol/feature-neg}
    \value LegacyOpenPgp \xep{0027}{Current Jabber OpenPGP Usage}: \c{jabber:x:encrypted}
    \value DiscoInfo \xep{0030}{Service Discovery}: \c{http://jabber.org/protocol/disco#info}
    \value DiscoItems \xep{0030}{Service Discovery}: \c{http://jabber.org/protocol/disco#items}
    \value ExtendedAddressing \xep{0033}{Extended Stanza Addressing}: \c{http://jabber.org/protocol/address}
    \value Muc \xep{0045}{Multi-User Chat}: \c{http://jabber.org/protocol/muc}
    \value MucAdmin \xep{0045}{Multi-User Chat}: \c{http://jabber.org/protocol/muc#admin}
    \value MucOwner \xep{0045}{Multi-User Chat}: \c{http://jabber.org/protocol/muc#owner}
    \value MucUser \xep{0045}{Multi-User Chat}: \c{http://jabber.org/protocol/muc#user}
    \value MucRoomInfo \xep{0045}{Multi-User Chat}: \c{http://jabber.org/protocol/muc#roominfo}
    \value MucRequest \xep{0045}{Multi-User Chat}: \c{http://jabber.org/protocol/muc#request}
    \value MucRoomConfig \xep{0045}{Multi-User Chat}: \c{http://jabber.org/protocol/muc#roomconfig}
    \value Ibb \xep{0047}{In-Band Bytestreams}: \c{http://jabber.org/protocol/ibb}
    \value Bookmarks \xep{0048}{Bookmarks}: \c{storage:bookmarks}
    \value PrivateXmlStorage \xep{0049}{Private XML Storage}: \c{jabber:iq:private}
    \value VCard \xep{0054}{vcard-temp}: \c{vcard-temp}
    \value Rsm \xep{0059}{Result Set Management}: \c{http://jabber.org/protocol/rsm}
    \value PubSub \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub}
    \value PubSubAutoCreate \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#auto-create}
    \value PubSubConfigNode \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#config-node}
    \value PubSubConfigNodeMax \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#config-node-max}
    \value PubSubCreateAndConfigure \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#create-and-configure}
    \value PubSubCreateNodes \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#create-nodes}
    \value PubSubErrors \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#errors}
    \value PubSubEvent \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#event}
    \value PubSubMultiItems \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#multi-items}
    \value PubSubNodeConfig \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#node_config}
    \value PubSubOwner \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#owner}
    \value PubSubPublish \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#publish}
    \value PubSubPublishOptions \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#publish-options}
    \value PubSubRsm \xep{0060}{Publish-Subscribe}: \c{http://jabber.org/protocol/pubsub#rsm}
    \value Bytestreams \xep{0065}{SOCKS5 Bytestreams}: \c{http://jabber.org/protocol/bytestreams}
    \value OutOfBandData \xep{0066}{Out of Band Data}: \c{jabber:x:oob}
    \value Xhtml \xep{0071}{XHTML-IM}: \c{http://www.w3.org/1999/xhtml}
    \value XhtmlIm \xep{0071}{XHTML-IM}: \c{http://jabber.org/protocol/xhtml-im}
    \value Register \xep{0077}{In-Band Registration}: \c{jabber:iq:register}
    \value RegisterFeature \xep{0077}{In-Band Registration}: \c{http://jabber.org/features/iq-register}
    \value Auth \xep{0078}{Non-SASL Authentication}: \c{jabber:iq:auth}
    \value AuthFeature \xep{0078}{Non-SASL Authentication}: \c{http://jabber.org/features/iq-auth}
    \value Geoloc \xep{0080}{User Location}: \c{http://jabber.org/protocol/geoloc}
    \value GeolocNotify \xep{0080}{User Location}: \c{http://jabber.org/protocol/geoloc+notify}
    \value UserAvatarData \xep{0084}{User Avatar}: \c{urn:xmpp:avatar:data}
    \value UserAvatarMetadata \xep{0084}{User Avatar}: \c{urn:xmpp:avatar:metadata}
    \value ChatStates \xep{0085}{Chat State Notifications}: \c{http://jabber.org/protocol/chatstates}
    \value LegacyDelayedDelivery \xep{0091}{Legacy Delayed Delivery}: \c{jabber:x:delay}
    \value SoftwareVersion \xep{0092}{Software Version}: \c{jabber:iq:version}
    \value DataForms \xep{0004}{Data Forms}: \c{jabber:x:data}
    \value StreamInitiation \xep{0095}{Stream Initiation}: \c{http://jabber.org/protocol/si}
    \value StreamInitiationFileTransfer \xep{0095}{Stream Initiation}: \c{http://jabber.org/protocol/si/profile/file-transfer}
    \value UrlData \xep{0103}{URL Address Information}: \c{http://jabber.org/protocol/url-data}
    \value Activity \xep{0108}{User Activity}: \c{http://jabber.org/protocol/activity}
    \value Capabilities \xep{0115}{Entity Capabilities}: \c{http://jabber.org/protocol/caps}
    \value Tune \xep{0118}{User Tune}: \c{http://jabber.org/protocol/tune}
    \value TuneNotify \xep{0118}{User Tune}: \c{http://jabber.org/protocol/tune+notify}
    \value Archive \xep{0136}{Message Archiving}: \c{urn:xmpp:archive}
    \value Compress \xep{0138}{Stream Compression}: \c{http://jabber.org/protocol/compress}
    \value CompressFeature \xep{0138}{Stream Compression}: \c{http://jabber.org/features/compress}
    \value RosterNotes \xep{0145}{Annotations}: \c{storage:rosternotes}
    \value VCardUpdate \xep{0153}{vCard-Based Avatars}: \c{vcard-temp:x:update}
    \value ContactAddresses \xep{0157}{Contact Addresses for XMPP Services}: \c{http://jabber.org/network/serverinfo}
    \value Captcha \xep{0158}{CAPTCHA Forms}: \c{urn:xmpp:captcha}
    \value Jingle1 \xep{0166}{Jingle}: \c{urn:xmpp:jingle:1}
    \value JingleRawUdp1 \xep{0166}{Jingle}: \c{urn:xmpp:jingle:transports:raw-udp:1}
    \value JingleIceUdp1 \xep{0166}{Jingle}: \c{urn:xmpp:jingle:transports:ice-udp:1}
    \value JingleErrors1 \xep{0166}{Jingle}: \c{urn:xmpp:jingle:errors:1}
    \value JingleRtp1 \xep{0167}{Jingle RTP Sessions}: \c{urn:xmpp:jingle:apps:rtp:1}
    \value JingleRtpAudio \xep{0167}{Jingle RTP Sessions}: \c{urn:xmpp:jingle:apps:rtp:audio}
    \value JingleRtpVideo \xep{0167}{Jingle RTP Sessions}: \c{urn:xmpp:jingle:apps:rtp:video}
    \value JingleRtpInfo1 \xep{0167}{Jingle RTP Sessions}: \c{urn:xmpp:jingle:apps:rtp:info:1}
    \value JingleRtpErrors1 \xep{0167}{Jingle RTP Sessions}: \c{urn:xmpp:jingle:apps:rtp:errors:1}
    \value MessageReceipts \xep{0184}{Message Receipts}: \c{urn:xmpp:receipts}
    \value Blocking \xep{0191}{Blocking Command}: \c{urn:xmpp:blocking}
    \value StreamManagement3 \xep{0198}{Stream Management}: \c{urn:xmpp:sm:3}
    \value Ping \xep{0199}{XMPP Ping}: \c{urn:xmpp:ping}
    \value EntityTime \xep{0202}{Entity Time}: \c{urn:xmpp:time}
    \value DelayedDelivery \xep{0203}{Delayed Delivery}: \c{urn:xmpp:delay}
    \value ExternalServiceDiscovery2 \xep{0215}{External Service Discovery}: \c{urn:xmpp:extdisco:2}
    \value ServerDialback \xep{0220}{Server Dialback}: \c{jabber:server:dialback}
    \value MediaElement \xep{0221}{Data Forms Media Element}: \c{urn:xmpp:media-element}
    \value Attention0 \xep{0224}{Attention}: \c{urn:xmpp:attention:0}
    \value BitsOfBinary \xep{0231}{Bits of Binary}: \c{urn:xmpp:bob}
    \value DirectMucInvitation \xep{0249}{Direct MUC Invitations}: \c{jabber:x:conference}
    \value Thumbnails1 \xep{0264}{Jingle Content Thumbnails}: \c{urn:xmpp:thumbs:1}
    \value Muji0 \xep{0272}{Multiparty Jingle (Muji)}: \c{urn:xmpp:jingle:muji:0}
    \value Carbons2 \xep{0280}{Message Carbons}: \c{urn:xmpp:carbons:2}
    \value Moved1 \xep{0283}{Moved}: \c{urn:xmpp:moved:1}
    \value JingleRtcpFeedback0 \xep{0293}{Jingle RTP Feedback Negotiation}: \c{urn:xmpp:jingle:apps:rtp:rtcp-fb:0}
    \value JingleRtpHeaderExtensions0 \xep{0294}{Jingle RTP Header Extensions Negotiation}: \c{urn:xmpp:jingle:apps:rtp:rtp-hdrext:0}
    \value Forwarding0 \xep{0297}{Stanza Forwarding}: \c{urn:xmpp:forward:0}
    \value Hashes2 \xep{0300}{Use of Cryptographic Hash Functions in XMPP}: \c{urn:xmpp:hashes:2}
    \value MucUnique \xep{0307}{Unique Room Names for Multi-User Chat}: \c{http://jabber.org/protocol/muc#unique}
    \value MessageCorrection0 \xep{0308}{Last Message Correction}: \c{urn:xmpp:message-correct:0}
    \value Mam2 \xep{0313}{Message Archive Management}: \c{urn:xmpp:mam:2}
    \value Idle1 \xep{0319}{Last User Interaction in Presence}: \c{urn:xmpp:idle:1}
    \value JingleDtls0 \xep{0320}{Use of DTLS-SRTP in Jingle Sessions}: \c{urn:xmpp:jingle:apps:dtls:0}
    \value ChatMarkers0 \xep{0333}{Chat Markers}: \c{urn:xmpp:chat-markers:0}
    \value MessageProcessingHints \xep{0334}{Message Processing Hints}: \c{urn:xmpp:hints}
    \value Csi0 \xep{0352}{Client State Indication}: \c{urn:xmpp:csi:0}
    \value JingleMessage0 \xep{0353}{Jingle Message Initiation}: \c{urn:xmpp:jingle-message:0}
    \value Push0 \xep{0357}{Push Notifications}: \c{urn:xmpp:push:0}
    \value StanzaId0 \xep{0359}{Unique and Stable Stanza IDs}: \c{urn:xmpp:sid:0}
    \value HttpUpload0 \xep{0363}{HTTP File Upload}: \c{urn:xmpp:http:upload:0}
    \value Otr0 \xep{0364}{Current Off-the-Record Messaging Usage}: \c{urn:xmpp:otr:0}
    \value MessageAttaching1 \xep{0367}{Message Attaching}: \c{urn:xmpp:message-attaching:1}
    \value MixCore1 \xep{0369}{Mediated Information eXchange (MIX)}: \c{urn:xmpp:mix:core:1}
    \value MixCore1CreateChannel \xep{0369}{Mediated Information eXchange (MIX)}: \c{urn:xmpp:mix:core:1#create-channel}
    \value MixCore1Searchable \xep{0369}{Mediated Information eXchange (MIX)}: \c{urn:xmpp:mix:core:1#searchable}
    \value MixNodesInfo \xep{0369}{Mediated Information eXchange (MIX)}: \c{urn:xmpp:mix:nodes:info}
    \value MixNodesMessages \xep{0369}{Mediated Information eXchange (MIX)}: \c{urn:xmpp:mix:nodes:messages}
    \value MixNodesParticipants \xep{0369}{Mediated Information eXchange (MIX)}: \c{urn:xmpp:mix:nodes:participants}
    \value OpenPgp0 \xep{0373}{OpenPGP for XMPP}: \c{urn:xmpp:openpgp:0}
    \value Reporting1 \xep{0377}{Blocking Command Reports}: \c{urn:xmpp:reporting:1}
    \value Eme0 \xep{0380}{Explicit Message Encryption}: \c{urn:xmpp:eme:0}
    \value Spoiler0 \xep{0382}{Spoiler messages}: \c{urn:xmpp:spoiler:0}
    \value LegacyOmemo \xep{0384}{OMEMO Encryption}: \c{eu.siacs.conversations.axolotl}
    \value Omemo1 \xep{0384}{OMEMO Encryption}: \c{urn:xmpp:omemo:1}
    \value Omemo2 \xep{0384}{OMEMO Encryption}: \c{urn:xmpp:omemo:2}
    \value Omemo2Bundles \xep{0384}{OMEMO Encryption}: \c{urn:xmpp:omemo:2:bundles}
    \value Omemo2Devices \xep{0384}{OMEMO Encryption}: \c{urn:xmpp:omemo:2:devices}
    \value Bind2_0 \xep{0386}{Bind 2}: \c{urn:xmpp:bind:0}
    \value Sasl2_2 \xep{0388}{Extensible SASL Profile}: \c{urn:xmpp:sasl:2}
    \value Bookmarks2_1 \xep{0402}{PEP Native Bookmarks}: \c{urn:xmpp:bookmarks:1}
    \value MixNodesPresence \xep{0403}{Mediated Information eXchange (MIX): Presence Support}: \c{urn:xmpp:mix:nodes:presence}
    \value MixNodesJidMap \xep{0404}{Mediated Information eXchange (MIX): JID Hidden Channels}: \c{urn:xmpp:mix:nodes:jidmap}
    \value MixPam2 \xep{0405}{Mediated Information eXchange (MIX): Participant Server Requirements}: \c{urn:xmpp:mix:pam:2}
    \value MixPam2Archive \xep{0405}{Mediated Information eXchange (MIX): Participant Server Requirements}: \c{urn:xmpp:mix:pam:2#archive}
    \value MixRoster0 \xep{0405}{Mediated Information eXchange (MIX): Participant Server Requirements}: \c{urn:xmpp:mix:roster:0}
    \value MixPresence0 \xep{0405}{Mediated Information eXchange (MIX): Participant Server Requirements}: \c{urn:xmpp:presence:0}
    \value MixAdmin0 \xep{0406}{Mediated Information eXchange (MIX): MIX Administration}: \c{urn:xmpp:mix:admin:0}
    \value MixNodesAllowed \xep{0406}{Mediated Information eXchange (MIX): MIX Administration}: \c{urn:xmpp:mix:nodes:allowed}
    \value MixNodesBanned \xep{0406}{Mediated Information eXchange (MIX): MIX Administration}: \c{urn:xmpp:mix:nodes:banned}
    \value MixNodesConfig \xep{0406}{Mediated Information eXchange (MIX): MIX Administration}: \c{urn:xmpp:mix:nodes:config}
    \value MixMisc0 \xep{0407}{Mediated Information eXchange (MIX): Miscellaneous Capabilities}: \c{urn:xmpp:mix:misc:0}
    \value MucSelfPingOptimization \xep{0410}{MUC Self-Ping (Schrödinger's Chat)}: \c{http://jabber.org/protocol/muc#self-ping-optimization}
    \value OccupantId0 \xep{0421}{Occupant identifiers for semi-anonymous MUCs}: \c{urn:xmpp:occupant-id:0}
    \value MessageRetract1 \xep{0424}{Message Retraction}: \c{urn:xmpp:message-retract:1}
    \value MessageRetract1Tombstone \xep{0424}{Message Retraction}: \c{urn:xmpp:message-retract:1#tombstone}
    \value MessageModerate1 \xep{0425}{Moderated Message Retraction}: \c{urn:xmpp:message-moderate:1}
    \value Fallback0 \xep{0428}{Fallback Indication}: \c{urn:xmpp:fallback:0}
    \value TrustMessages1 \xep{0434}{Trust Messages (TM)}: \c{urn:xmpp:tm:1}
    \value Reactions0 \xep{0444}{Message Reactions}: \c{urn:xmpp:reactions:0}
    \value FileMetadata0 \xep{0446}{File metadata element}: \c{urn:xmpp:file:metadata:0}
    \value StatelessFileSharing0 \xep{0447}{Stateless file sharing}: \c{urn:xmpp:sfs:0}
    \value EncryptedStatelessFileSharing0 \xep{0448}{Encryption for stateless file sharing}: \c{urn:xmpp:esfs:0}
    \value Atm1 \xep{0450}{Automatic Trust Management (ATM)}: \c{urn:xmpp:atm:1}
    \value Reply0 \xep{0461}{Message Replies}: \c{urn:xmpp:reply:0}
    \value CallInvites0 \xep{0482}{Call Invites}: \c{urn:xmpp:call-invites:0}
    \value Fast0 \xep{0484}{Fast Authentication Streamlining Tokens}: \c{urn:xmpp:fast:0}

    \since QXmpp 1.17
*/

/*!
    Returns the URI of the namespace \a ns.

    \since QXmpp 1.17
*/
QString QXmpp::namespaceUri(Namespace ns)
{
    return Enums::toString(ns);
}

/*!
    Returns the namespace with the URI \a uri or nothing if the namespace is not known to QXmpp.

    \since QXmpp 1.17
*/
std::optional<Namespace> QXmpp::namespaceFromUri(QStringView uri)
{
    return Enums::fromString<Namespace>(uri);
}
