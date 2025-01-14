/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <drawinglayer/attribute/fillgradientattribute.hxx>
#include <basegfx/utils/gradienttools.hxx>

namespace drawinglayer::attribute
{
        class ImpFillGradientAttribute
        {
        public:
            // data definitions
            double                                  mfBorder;
            double                                  mfOffsetX;
            double                                  mfOffsetY;
            double                                  mfAngle;
            basegfx::ColorSteps                     maColorSteps;
            GradientStyle                           meStyle;
            sal_uInt16                              mnSteps;

            ImpFillGradientAttribute(
                GradientStyle eStyle,
                double fBorder,
                double fOffsetX,
                double fOffsetY,
                double fAngle,
                const basegfx::BColor& rStartColor,
                const basegfx::BColor& rEndColor,
                const basegfx::ColorSteps* pColorSteps,
                sal_uInt16 nSteps)
            :   mfBorder(fBorder),
                mfOffsetX(fOffsetX),
                mfOffsetY(fOffsetY),
                mfAngle(fAngle),
                maColorSteps(),
                meStyle(eStyle),
                mnSteps(nSteps)
            {
                // always add start color to guarantee a color at all. It's also just safer
                // to have one and not an empty vector, that spares many checks in the using code
                maColorSteps.emplace_back(0.0, rStartColor);

                // if we have given ColorSteps, integrate these
                if(nullptr != pColorSteps && !pColorSteps->empty())
                {
                    // append early to local & sort to prepare the following correction(s)
                    // and later processing
                    maColorSteps.insert(maColorSteps.end(), pColorSteps->begin(), pColorSteps->end());

                    // no need to sort knowingly lowest entry StartColor, also guarantees that
                    // entry to stay at begin
                    std::sort(maColorSteps.begin() + 1, maColorSteps.end());

                    // use two r/w heads on the data band maColorSteps
                    size_t curr(0), next(1);

                    // during processing, check if all colors are the same. We know the
                    // StartColor, so to all be the same, all also have to be equal to
                    // StartColor (including EndColor, use to initialize)
                    bool bAllTheSameColor(rStartColor == rEndColor);

                    // remove entries <= 0.0, >= 1.0 and with equal offset. do
                    // this inside the already sorted local vector by evtl.
                    // moving entries closer to begin to keep and adapting size
                    // at the end
                    for(; next < maColorSteps.size(); next++)
                    {
                        const double fNextOffset(maColorSteps[next].getOffset());

                        // check for < 0.0 (should not really happen, see ::ColorStep)
                        // also check for == 0.0 which would mean than an implicit
                        // StartColor was given in ColorSteps - ignore that, we want
                        // the explicitly given StartColor to always win
                        if(basegfx::fTools::lessOrEqual(fNextOffset, 0.0))
                            continue;

                        // check for > 1.0 (should not really happen, see ::ColorStep)
                        // also check for == 1.0 which would mean than an implicit
                        // EndColor was given in ColorSteps - ignore that, we want
                        // the explicitly given EndColor to always win
                        if(basegfx::fTools::moreOrEqual(fNextOffset, 1.0))
                            continue;

                        // check for equal current offset
                        const double fCurrOffset(maColorSteps[curr].getOffset());
                        if(basegfx::fTools::equal(fNextOffset, fCurrOffset))
                            continue;

                        // next is > 0.0, < 1.0 and != curr, so a valid entry.
                        // take over by evtl. have to move it left
                        curr++;
                        if(curr != next)
                        {
                            maColorSteps[curr] = maColorSteps[next];
                        }

                        // new valid entry detected, check it for all the same color
                        bAllTheSameColor = bAllTheSameColor && maColorSteps[curr].getColor() == rStartColor;
                    }

                    if(bAllTheSameColor)
                    {
                        // if all are the same color, reset to StartColor only
                        maColorSteps.resize(1);
                    }
                    else
                    {
                        // adapt size to detected useful entries
                        curr++;
                        if(curr != maColorSteps.size())
                        {
                            maColorSteps.resize(curr);
                        }

                        // add EndColor if in-between colors were added
                        // or StartColor != EndColor
                        if(curr > 1 || rStartColor != rEndColor)
                        {
                            maColorSteps.emplace_back(1.0, rEndColor);
                        }
                    }
                }
                else
                {
                    // add EndColor if different from StartColor
                    if(rStartColor != rEndColor)
                    {
                        maColorSteps.emplace_back(1.0, rEndColor);
                    }
                }
            }

            ImpFillGradientAttribute()
            :   mfBorder(0.0),
                mfOffsetX(0.0),
                mfOffsetY(0.0),
                mfAngle(0.0),
                maColorSteps(),
                meStyle(GradientStyle::Linear),
                mnSteps(0)
            {
                // always add a fallback color, see above
                maColorSteps.emplace_back(0.0, basegfx::BColor());
            }

            // data read access
            GradientStyle getStyle() const { return meStyle; }
            double getBorder() const { return mfBorder; }
            double getOffsetX() const { return mfOffsetX; }
            double getOffsetY() const { return mfOffsetY; }
            double getAngle() const { return mfAngle; }
            const basegfx::ColorSteps& getColorSteps() const { return maColorSteps; }
            sal_uInt16 getSteps() const { return mnSteps; }

            bool hasSingleColor() const
            {
                // No entry (should not happen, see comments for startColor above)
                // or single entry -> no gradient.
                // No need to check for all-the-same color since this is checked/done
                // in the constructor already, see there
                return maColorSteps.size() < 2;
            }

            bool operator==(const ImpFillGradientAttribute& rCandidate) const
            {
                return (getStyle() == rCandidate.getStyle()
                    && getBorder() == rCandidate.getBorder()
                    && getOffsetX() == rCandidate.getOffsetX()
                    && getOffsetY() == rCandidate.getOffsetY()
                    && getAngle() == rCandidate.getAngle()
                    && getColorSteps() == rCandidate.getColorSteps()
                    && getSteps() == rCandidate.getSteps());
            }
        };

        namespace
        {
            FillGradientAttribute::ImplType& theGlobalDefault()
            {
                static FillGradientAttribute::ImplType SINGLETON;
                return SINGLETON;
            }
        }

        FillGradientAttribute::FillGradientAttribute(
            GradientStyle eStyle,
            double fBorder,
            double fOffsetX,
            double fOffsetY,
            double fAngle,
            const basegfx::BColor& rStartColor,
            const basegfx::BColor& rEndColor,
            const basegfx::ColorSteps* pColorSteps,
            sal_uInt16 nSteps)
        :   mpFillGradientAttribute(ImpFillGradientAttribute(
                eStyle, fBorder, fOffsetX, fOffsetY, fAngle, rStartColor, rEndColor, pColorSteps, nSteps))
        {
        }

        FillGradientAttribute::FillGradientAttribute()
        :   mpFillGradientAttribute(theGlobalDefault())
        {
        }

        FillGradientAttribute::FillGradientAttribute(const FillGradientAttribute&) = default;

        FillGradientAttribute::FillGradientAttribute(FillGradientAttribute&&) = default;

        FillGradientAttribute::~FillGradientAttribute() = default;

        bool FillGradientAttribute::isDefault() const
        {
            return mpFillGradientAttribute.same_object(theGlobalDefault());
        }

        bool FillGradientAttribute::hasSingleColor() const
        {
            return mpFillGradientAttribute->hasSingleColor();
        }

        FillGradientAttribute& FillGradientAttribute::operator=(const FillGradientAttribute&) = default;

        FillGradientAttribute& FillGradientAttribute::operator=(FillGradientAttribute&&) = default;

        bool FillGradientAttribute::operator==(const FillGradientAttribute& rCandidate) const
        {
            // tdf#87509 default attr is always != non-default attr, even with same values
            if(rCandidate.isDefault() != isDefault())
                return false;

            return rCandidate.mpFillGradientAttribute == mpFillGradientAttribute;
        }

        const basegfx::ColorSteps& FillGradientAttribute::getColorSteps() const
        {
            return mpFillGradientAttribute->getColorSteps();
        }

        double FillGradientAttribute::getBorder() const
        {
            return mpFillGradientAttribute->getBorder();
        }

        double FillGradientAttribute::getOffsetX() const
        {
            return mpFillGradientAttribute->getOffsetX();
        }

        double FillGradientAttribute::getOffsetY() const
        {
            return mpFillGradientAttribute->getOffsetY();
        }

        double FillGradientAttribute::getAngle() const
        {
            return mpFillGradientAttribute->getAngle();
        }

        GradientStyle FillGradientAttribute::getStyle() const
        {
            return mpFillGradientAttribute->getStyle();
        }

        sal_uInt16 FillGradientAttribute::getSteps() const
        {
            return mpFillGradientAttribute->getSteps();
        }

} // end of namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
