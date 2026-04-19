# Scope — Rules of Engagement

## Rules of Usage

**This project is a lab instrument for professional security research and education.**
It is not a product, not a tool for operational attack, and not intended for
deployment against any network or device outside the operator's sole ownership.

The techniques implemented here — rogue access point, captive portal, HTTP form
capture — have been publicly documented and taught in the security research
community for over a decade, including at venues such as **DEF CON** (Wireless
Village) and **Black Hat Europe**. They are foundational curriculum for:

- Red team operators assessing wireless perimeter under authorized engagement
- Blue team / SOC analysts learning to detect rogue APs and portal hijacking
- Academic researchers studying user trust models in captive-portal UX
- Security students (self-directed and formal programs) building intuition
  for why HTTPS, HSTS, and certificate pinning exist

The working assumption of this codebase is that **the reader already knows why
HTTPS matters** and is building this lab to *feel* the attack from the other
side — so that defensive work (detection rules, user-education material,
product hardening) is grounded in mechanics, not slides.

### Intended audience

- Professional security researchers operating within a documented lab scope
- Students and self-taught practitioners with a single, isolated ESP32 and
  their own personal devices
- Educators demonstrating plaintext-protocol weaknesses in a controlled setting

### Not the intended audience

- Anyone seeking to intercept traffic from people who have not consented
- Anyone running this in a shared WiFi environment (dorm, office, café, public
  space) where non-consenting devices may associate
- Anyone expecting an "out-of-the-box attack tool" — it is deliberately
  educational and makes no attempt to hide its AP name, defeat HTTPS, or
  persist captured data off-device

## Authorized

- Target devices: personal devices only (own phone, own laptop)
- Network: isolated ESP32 AP (no internet bridge)
- Data: dummy/test credentials only — never real accounts, even your own
- Location: private space (home/room), out of range of any non-consenting device
- Duration: powered on only during active research sessions; powered off
  between sessions so nothing associates passively

## Not Authorized

- Broadcasting the AP in public areas (campus, café, mall, transit, workplace)
- Cloning existing WiFi SSIDs (Evil Twin against a real network)
- Targeting any device not owned by the operator
- Capturing real credentials from any person, including the operator's own
  production accounts
- Bridging the AP to the internet (STA mode forwarding real traffic)
- Redistributing captured data (screenshots of the dashboard showing real
  credentials count as captured data)

## Legal Reference (Indonesia)

UU ITE No. 11/2008 jo. UU 19/2016:

- **Pasal 30** — Unauthorized access to computer systems
- **Pasal 31** — Unauthorized interception of electronic information
- **Pasal 32** — Unauthorized modification of electronic information
- **Pasal 33** — Causing disruption to electronic systems

Violation carries imprisonment up to 8 years and/or fine up to IDR 800 million.

International operators: analogous statutes apply (US CFAA 18 U.S.C. § 1030,
UK Computer Misuse Act 1990, EU Directive 2013/40/EU). The legality of the
*techniques* at a conference talk does not extend to *deployment* against
third parties. Research ≠ authorization.

## Acknowledgment

This tool exists to understand attack techniques for the purpose of building
better defenses. The operator accepts full responsibility for any use outside
the authorized scope defined above. If you are unsure whether a specific use
case is in scope — **it isn't**. Default to not running it.
