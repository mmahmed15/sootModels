/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2013-2016 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Class
    Foam::radiation::YaoSootModelTurbulent

Description
    This soot model is based on fuel Turbulent smoke point height LSP, the model was
    originally developed by W. Yao et al.,FireSafetyJournal 46(2011)371–387.

    NOTE: This current version ignores turbulence-soot interactions

    This class includes:
    	- direct calculation of mixutre fraction Z
    	- solution of a transport equation of soot mass fraction Ysoot

    The input dictionary reads like in the radiationProperties dictionary:

            sootModel YaoSootModelTurbulent<gasHThermoPhysics>;

            YaoSootModelTurbulentCoeffs
            {
                Xtilde_resolution 1e-3;
                Xvar_resolution 1e-4;
                Xtilde_max  1.0;
                Xvar_max   0.25;

                solveSoot   true;

                SGSFilter   true;
                
                rhoSoot     1800;

                //fuel specific parameters
                Af          4e-5;
                Z_sf        0.15;
                Z_so        0.1;

                //reference quantities
                rho_ref     1.172;
                MW_ref      28.8e-3;
                T_ref       298;
                T_ad        2375;

                proportionalityConst        0.4;                
            }

SourceFiles
    YaoSootModelTurbulent.C

\*---------------------------------------------------------------------------*/

#ifndef YaoSootModelTurbulent_H
#define YaoSootModelTurbulent_H

#include "fvCFD.H"
#include "sootModel.H"
#include "fluidThermo.H"
#include "thermoPhysicsTypes.H"
#include "psiReactionThermo.H"
#include "reactingMixture.H"
#include "singleStepReactingMixture.H"
#include "turbulentFluidThermoModel.H"
#include "interpolation2DTable.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

namespace radiation
{

/*---------------------------------------------------------------------------*\
                    Class YaoSootModelTurbulent Declaration
\*---------------------------------------------------------------------------*/
template<class ThermoType>
class YaoSootModelTurbulent
:
    public sootModel
{

    // Static functions

        //- Check mixture in thermo
        static const reactingMixture<ThermoType>& checkThermo
        (
            const fluidThermo&
        );


    // Private data

        //- Thermo packages
        const psiReactionThermo& thermo;

         //- Soot mass fraction
        volScalarField Ysoot;

        //- Soot volume fraction
        volScalarField fv;

        //- Mean soot source terms
        volScalarField sootFormationRate;
        volScalarField sootOxidationRate; 

        //- Soot time derivative and convection terms
        volScalarField sootConvection;
        volScalarField sootTimeDer;
        volScalarField diffusion;

        //- Mixture fraction
        volScalarField Z;
        volScalarField Zvar;

        //- Normalized T
        volScalarField Theta;
        volScalarField ThetaVar;

        //- filtered density
        volScalarField rhoBar;

        //- Oxygen concentration
        volScalarField O2Concentration;

        //- Soot model dictionary
        dictionary coeffsDict_;

        // - Solve switch
        Switch solveSoot;
        Switch SGSFilter;

        // - Soot density [kg/m3]
        dimensionedScalar rhoSoot;

        //- Formation pre-exponential factor
        scalar Af;

        //- Fuel stoich. ratio
        scalar s;

        //- Index of oxygen and fuel
        label O2Index;      
        label fuelIndex; 

        // - Mass fraction of oxygen in oxidizer stream
        scalar YO2Inf;  

        //- Fuel mass fraction in fuel supply stream
        scalar YFInf;

        //- Stoic. mixture fraction
        scalar Z_st;

        //- Formation and oxidation mixture frac. limits
        scalar Z_sf;
        scalar Z_so;

        //- Formation rate parameters
        scalar gamma;
        scalar Ta;

        //- Oxidation rate parameters
        scalar Asoot;
        scalar Aox;
        scalar EaOx;

        //- Constants
        dimensionedScalar MW_O2;
        dimensionedScalar MW_Fuel;
        dimensionedScalar Ru;
        
        //- Reference quantities for Beta-PDF integration
        scalar rho_ref;
        scalar MW_ref;
        scalar T_ref;
        scalar T_ad;

        // - Temperature variance proportionality const.
        scalar proportionalityConst;

        //- Lookup table resolution and bounds
        scalar dX;
        scalar dXVar;
        scalar X_max;
        scalar XVar_max;

        //- Lookup table of Beta-PDF integration functions
        autoPtr<interpolation2DTable<scalar>> lookup_SF_Z;
        autoPtr<interpolation2DTable<scalar>> lookup_SF_Theta;

        autoPtr<interpolation2DTable<scalar>> lookup_SO_Z;
        autoPtr<interpolation2DTable<scalar>> lookup_SO_Theta;
        
        autoPtr<interpolation2DTable<scalar>> lookup_invRho_Z;
        autoPtr<interpolation2DTable<scalar>> lookup_invRho_Theta;


    // Private member functions

        //integratio of: F(Z) Ptilde(Z) dZ
        double integratePDF(
                const word& sourceName,            
                const double& etaTilde,
                const double& etaVar
                );

        //return mixture fraction (or temperature) dependent parts of the surce term
        double sourceFunc(
                const word& sourceName,
                const double& eta 
                );

        //generate lookup table of integrated PDF
        void generateLookup(const word& sourceName);

public:

    //- Runtime type information
    TypeName("YaoSootModelTurbulent");


    // Constructors

        //- Construct from components
        YaoSootModelTurbulent
        (
            const dictionary& dict,
            const fvMesh& mesh,
            const word& modelType
        );


    //- Destructor
    virtual ~YaoSootModelTurbulent();


    // Member Functions

        // Edit

            //- Main update/correction routine
            virtual void correct();         

        // Access

            //- Return soot mass fraction
            const volScalarField& soot() const
            {
                return Ysoot;
            }

};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam
} // End namespace radiation

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#ifdef NoRepository
    #include "YaoSootModelTurbulent.C"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
