/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mtproto/web_proxy/web_proxy_frame.h"
#include "mtproto/mtproto_proxy_data.h"

#include "tests/test_named_checks.hpp"

#include <cstdio>

namespace {

using MTP::WebProxy::ApplyWindowCredit;
using MTP::WebProxy::Frame;
using MTP::WebProxy::FrameType;
using MTP::WebProxy::kFrameHeaderSize;
using MTP::WebProxy::kInitialStreamWindow;
using MTP::WebProxy::kMaxBatchFrames;
using MTP::WebProxy::ParseFrames;
using MTP::WebProxy::SerializeFrame;
using MTP::DecodeWebProxyLinkSecret;
using MTP::EncodeWebProxyLinkSecret;
using MTP::NormalizeWebProxyHost;
using MTP::ProxyData;
using MTP::WebProxyBridgeCapability;
using MTP::WebProxyBridgePath;
using MTP::WebProxyBridgeUrl;
using Test::NamedChecks::Check;

[[nodiscard]] QString CheckSerializedFrame() {
	const auto frame = SerializeFrame(
		FrameType::Data,
		0x123456,
		QByteArray("abc"));
	return (frame.toHex() == "0212345600000003616263")
		? QString()
		: u"serialized DATA frame has an unexpected wire format"_q;
}

[[nodiscard]] QString CheckPartialFrame() {
	const auto encoded = SerializeFrame(
		FrameType::Data,
		7,
		QByteArray("payload"));
	auto buffer = encoded.left(5);
	auto frames = std::vector<Frame>();
	if (!ParseFrames(buffer, frames)
		|| !frames.empty()
		|| buffer != encoded.left(5)) {
		return u"incomplete header was consumed or emitted"_q;
	}
	buffer.append(encoded.mid(5));
	if (!ParseFrames(buffer, frames)
		|| !buffer.isEmpty()
		|| frames.size() != 1) {
		return u"completed frame was not decoded"_q;
	}
	return (frames.front().type == FrameType::Data
		&& frames.front().streamId == 7
		&& frames.front().payload == "payload")
		? QString()
		: u"decoded frame fields do not match the source"_q;
}

[[nodiscard]] QString CheckBatchWithPartialTail() {
	const auto first = SerializeFrame(FrameType::Open, 1);
	const auto second = SerializeFrame(FrameType::Close, 1);
	const auto third = SerializeFrame(
		FrameType::Data,
		2,
		QByteArray("tail"));
	auto buffer = first + second + third.left(kFrameHeaderSize + 2);
	auto frames = std::vector<Frame>();
	if (!ParseFrames(buffer, frames)
		|| frames.size() != 2
		|| buffer != third.left(kFrameHeaderSize + 2)) {
		return u"complete batch frames were not separated from an incomplete tail"_q;
	}
	buffer.append(third.mid(kFrameHeaderSize + 2));
	if (!ParseFrames(buffer, frames)
		|| !buffer.isEmpty()
		|| frames.size() != 3) {
		return u"partial tail was not completed on the next parse"_q;
	}
	return (frames[0].type == FrameType::Open
		&& frames[1].type == FrameType::Close
		&& frames[2].type == FrameType::Data
		&& frames[2].payload == "tail")
		? QString()
		: u"batch frame order changed"_q;
}

[[nodiscard]] QString CheckMalformedBatchIsAtomic() {
	const auto seed = SerializeFrame(FrameType::Open, 99);
	auto output = std::vector<Frame>();
	auto input = seed;
	if (!ParseFrames(input, output) || output.size() != 1) {
		return u"could not prepare existing output state"_q;
}
	const auto beforeOutput = output.front();
	input = SerializeFrame(FrameType::Data, 1, QByteArray("valid"));
	input.append(QByteArray::fromHex("7f00000000000000"));
	const auto beforeInput = input;
	if (ParseFrames(input, output)) {
		return u"unknown frame type was accepted"_q;
	}
	return (input == beforeInput
		&& output.size() == 1
		&& output.front().type == beforeOutput.type
		&& output.front().streamId == beforeOutput.streamId)
		? QString()
		: u"failed parse changed the input or appended a partial batch"_q;
}

[[nodiscard]] QString CheckOversizedFrameIsRejected() {
	auto input = QByteArray::fromHex("0200000100100001");
	const auto before = input;
	auto frames = std::vector<Frame>();
	return (!ParseFrames(input, frames)
		&& input == before
		&& frames.empty())
		? QString()
		: u"frame larger than the protocol limit was not rejected atomically"_q;
}

[[nodiscard]] QString CheckBatchLimit() {
	auto input = QByteArray();
	for (auto i = 0; i != kMaxBatchFrames + 1; ++i) {
		input.append(SerializeFrame(FrameType::Open, uint32(i + 1)));
	}
	const auto before = input;
	auto frames = std::vector<Frame>();
	return (!ParseFrames(input, frames)
		&& input == before
		&& frames.empty())
		? QString()
		: u"batch larger than the frame limit was not rejected atomically"_q;
}

[[nodiscard]] QString CheckWindowCreditBoundaries() {
	auto window = uint32(0);
	if (!ApplyWindowCredit(window, uint32(kInitialStreamWindow))
		|| window != uint32(kInitialStreamWindow)) {
		return u"initial window credit was not accepted"_q;
	}
	if (ApplyWindowCredit(window, 1)
		|| window != uint32(kInitialStreamWindow)) {
		return u"credit above the 4 MiB limit was accepted"_q;
	}
	window = uint32(kInitialStreamWindow - 1024);
	if (!ApplyWindowCredit(window, 1024)
		|| window != uint32(kInitialStreamWindow)) {
		return u"credit exactly reaching the limit was rejected"_q;
	}
	if (ApplyWindowCredit(window, 1)
		|| ApplyWindowCredit(window, 0)
		|| window != uint32(kInitialStreamWindow)) {
		return u"overflow or zero credit changed the window"_q;
	}
	return QString();
}

[[nodiscard]] QString CheckWebProxyHostNormalization() {
	return (NormalizeWebProxyHost(u" Proxy.Example.COM "_q)
			== u"proxy.example.com"_q
		&& NormalizeWebProxyHost(u"bücher.example"_q)
			== u"xn--bcher-kva.example"_q
		&& NormalizeWebProxyHost(u"127.1"_q).isEmpty()
		&& NormalizeWebProxyHost(u"localhost"_q).isEmpty()
		&& NormalizeWebProxyHost(u"site.example:443"_q).isEmpty())
		? QString()
		: u"WEB proxy host canonicalization accepted an invalid host"_q;
}

[[nodiscard]] ProxyData PathProxy() {
	auto proxy = ProxyData();
	proxy.type = ProxyData::Type::Web;
	proxy.port = 443;
	proxy.setWebAddress(u" HTTPS://Proxy.Example.COM/dobry-cola-super-app/ "_q);
	proxy.password = u"000102030405060708090a0b0c0d0e0f"_q;
	return proxy;
}

[[nodiscard]] QString CheckWebProxyAddressAndCapability() {
	const auto proxy = PathProxy();
	return (proxy.valid()
		&& proxy.webAddress() == u"proxy.example.com/dobry-cola-super-app"_q
		&& WebProxyBridgePath(proxy) == u"/dobry-cola-super-app/"_q
		&& WebProxyBridgeCapability(proxy)
			== u"hHz99Xs93EN1j91G9gpNepXwGNNt5YdAFkEVk_LlqdQ"_q
		&& WebProxyBridgeUrl(proxy).startsWith(
			u"https://proxy.example.com/dobry-cola-super-app/?bridge="_q))
		? QString()
		: u"WEB proxy address or host/path-bound capability changed"_q;
}

[[nodiscard]] QString CheckWebProxyLinkSecretMarker() {
	const auto proxy = PathProxy();
	const auto encoded = EncodeWebProxyLinkSecret(proxy);
	const auto expected = QString::fromLatin1(QByteArray::fromHex(
		"70000102030405060708090a0b0c0d0e0f").toBase64(
			QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
	return (encoded == expected
		&& DecodeWebProxyLinkSecret(encoded, true) == proxy.password
		&& DecodeWebProxyLinkSecret(proxy.password, false) == proxy.password
		&& DecodeWebProxyLinkSecret(proxy.password, true).isEmpty())
		? QString()
		: u"WEB proxy path-aware link secret encoding changed"_q;
}

[[nodiscard]] QString CheckWebProxyInvalidPaths() {
	auto proxy = PathProxy();
	proxy.setWebAddress(u"proxy.example.com/valid//invalid"_q);
	return (!proxy.valid()
		&& proxy.host.isEmpty()
		&& proxy.user.isEmpty())
		? QString()
		: u"WEB proxy accepted a path containing an empty segment"_q;
}

[[nodiscard]] std::vector<Check> Checks() {
	return {
		{ "frame.serialize-wire-format", CheckSerializedFrame },
		{ "frame.partial-header", CheckPartialFrame },
		{ "frame.batch-and-partial-tail", CheckBatchWithPartialTail },
		{ "frame.malformed-batch-atomicity", CheckMalformedBatchIsAtomic },
		{ "frame.maximum-payload", CheckOversizedFrameIsRejected },
		{ "frame.maximum-batch", CheckBatchLimit },
		{ "flow-control.window-credit-boundaries", CheckWindowCreditBoundaries },
		{ "web-proxy.host-normalization", CheckWebProxyHostNormalization },
		{ "web-proxy.address-and-capability", CheckWebProxyAddressAndCapability },
		{ "web-proxy.link-secret-marker", CheckWebProxyLinkSecretMarker },
		{ "web-proxy.invalid-paths", CheckWebProxyInvalidPaths },
	};
}

} // namespace

int main() {
	return Test::NamedChecks::Run(Checks());
}
