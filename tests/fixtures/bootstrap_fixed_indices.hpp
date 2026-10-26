#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <string_view>
#include <vector>

namespace bootstrap_fixed_indices_fixture {

enum class expected_class { finite, nan, positive_infinity, negative_infinity };
struct expected_number { expected_class value_class; double value; };

inline constexpr double absolute_tolerance = 1.0e-10;
inline constexpr double relative_tolerance = 1.0e-9;

inline const std::vector<double> performance_returns {0.01, -0.02, 0.03, -0.01, 0.02, -0.03, 0.015, -0.005};
inline const std::vector<std::vector<std::size_t>> performance_draws {
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 0, 1, 1, 4, 4, 6, 6},
    {1, 2, 3, 4, 5, 6, 7, 0},
    {0, 2, 3, 4, 5, 5, 7, 7},
    {0, 1, 3, 4, 5, 6, 6, 7},
    {0, 1, 2, 4, 5, 5, 7, 0},
    {0, 2, 2, 3, 4, 6, 6, 7},
    {1, 1, 2, 3, 4, 5, 7, 7}
};
inline constexpr std::array<std::string_view, 13> performance_names{{
    "Annual return",
    "Cumulative returns",
    "Annual volatility",
    "Sharpe ratio",
    "Calmar ratio",
    "Stability",
    "Max drawdown",
    "Omega ratio",
    "Sortino ratio",
    "Skew",
    "Kurtosis",
    "Tail ratio",
    "Daily value at risk"
}};
inline const std::vector<std::vector<expected_number>> performance_samples_by_draw{
    {expected_number{expected_class::finite, 0.30601534809103126}, expected_number{expected_class::finite, 0.0085115998268927573}, expected_number{expected_class::finite, 0.33068111527572902}, expected_number{expected_class::finite, 0.95257934441568037}, expected_number{expected_class::finite, 10.200511603034379}, expected_number{expected_class::finite, 0.025861264914631196}, expected_number{expected_class::finite, -0.029999999999999992}, expected_number{expected_class::finite, 1.1538461538461537}, expected_number{expected_class::finite, 1.486783883350056}, expected_number{expected_class::finite, -0.13780376795472565}, expected_number{expected_class::finite, -1.2454402276075804}, expected_number{expected_class::finite, 0.99999999999999989}, expected_number{expected_class::finite, -0.026499999999999999}},
    {expected_number{expected_class::finite, 3.6629504404318576}, expected_number{expected_class::finite, 0.050091944631203367}, expected_number{expected_class::finite, 0.26410225292488515}, expected_number{expected_class::finite, 5.9635992595941811}, expected_number{expected_class::finite, 92.498748495753901}, expected_number{expected_class::finite, 0.31921034087932593}, expected_number{expected_class::finite, -0.039600000000000031}, expected_number{expected_class::finite, 2.25}, expected_number{expected_class::finite, 9.9215674164922163}, expected_number{expected_class::finite, -0.97940937524240212}, expected_number{expected_class::finite, -0.79808532778355801}, expected_number{expected_class::finite, 1.0}, expected_number{expected_class::finite, -0.02}},
    {expected_number{expected_class::finite, 0.30601534809102215}, expected_number{expected_class::finite, 0.0085115998268925352}, expected_number{expected_class::finite, 0.33068111527572902}, expected_number{expected_class::finite, 0.95257934441568037}, expected_number{expected_class::finite, 10.200511603034045}, expected_number{expected_class::finite, 0.092247681524577796}, expected_number{expected_class::finite, -0.030000000000000082}, expected_number{expected_class::finite, 1.1538461538461537}, expected_number{expected_class::finite, 1.486783883350056}, expected_number{expected_class::finite, -0.13780376795472563}, expected_number{expected_class::finite, -1.2454402276075811}, expected_number{expected_class::finite, 0.99999999999999989}, expected_number{expected_class::finite, -0.026499999999999999}},
    {expected_number{expected_class::finite, -0.49489317301417579}, expected_number{expected_class::finite, -0.021448707577233828}, expected_number{expected_class::finite, 0.34467375879228174}, expected_number{expected_class::finite, -1.8278153875348271}, expected_number{expected_class::finite, -7.2262498719407393}, expected_number{expected_class::finite, 0.51082659584061985}, expected_number{expected_class::finite, -0.068485477500000058}, expected_number{expected_class::finite, 0.74999999999999978}, expected_number{expected_class::finite, -2.5419556372089693}, expected_number{expected_class::finite, 0.083926016576817891}, expected_number{expected_class::finite, -1.1315426997245177}, expected_number{expected_class::finite, 0.88333333333333319}, expected_number{expected_class::finite, -0.029999999999999999}},
    {expected_number{expected_class::finite, -0.17727246334164726}, expected_number{expected_class::finite, -0.0061754623065087833}, expected_number{expected_class::finite, 0.2922113960816724}, expected_number{expected_class::finite, -0.53899335245631264}, expected_number{expected_class::finite, -4.4225110428181393}, expected_number{expected_class::finite, 0.037435883312232035}, expected_number{expected_class::finite, -0.040084119999999959}, expected_number{expected_class::finite, 0.92307692307692302}, expected_number{expected_class::finite, -0.74339194167502809}, expected_number{expected_class::finite, -0.38994920859391813}, expected_number{expected_class::finite, -1.2843298077874465}, expected_number{expected_class::finite, 0.68867924528301883}, expected_number{expected_class::finite, -0.026499999999999999}},
    {expected_number{expected_class::finite, -0.41225056055471043}, expected_number{expected_class::finite, -0.016730044931674626}, expected_number{expected_class::finite, 0.36481159521045931}, expected_number{expected_class::finite, -1.2951890954217493}, expected_number{expected_class::finite, -6.4611518083318433}, expected_number{expected_class::finite, 0.2928812105726355}, expected_number{expected_class::finite, -0.063804500000000208}, expected_number{expected_class::finite, 0.82352941176470573}, expected_number{expected_class::finite, -1.7847670039607322}, expected_number{expected_class::finite, -0.0538228880870152}, expected_number{expected_class::finite, -1.4426794351366823}, expected_number{expected_class::finite, 0.88333333333333319}, expected_number{expected_class::finite, -0.029999999999999999}},
    {expected_number{expected_class::finite, 25.125834783270612}, expected_number{expected_class::finite, 0.10913996637810319}, expected_number{expected_class::finite, 0.23213681310813244}, expected_number{expected_class::finite, 14.248063268014807}, expected_number{expected_class::finite, 2512.5834783270802}, expected_number{expected_class::finite, 0.90116915935516628}, expected_number{expected_class::finite, -0.0099999999999999239}, expected_number{expected_class::finite, 8.0000000000000018}, expected_number{expected_class::finite, 52.70958167164676}, expected_number{expected_class::finite, -0.410361998242186}, expected_number{expected_class::finite, -1.0061584459621424}, expected_number{expected_class::finite, 3.6363636363636362}, expected_number{expected_class::finite, -0.0082500000000000004}},
    {expected_number{expected_class::finite, -0.73010607852330511}, expected_number{expected_class::finite, -0.040726078143488276}, expected_number{expected_class::finite, 0.32588341473600646}, expected_number{expected_class::finite, -3.8664133951729589}, expected_number{expected_class::finite, -17.927237578608885}, expected_number{expected_class::finite, 0.11848087293175971}, expected_number{expected_class::finite, -0.040726078143488283}, expected_number{expected_class::finite, 0.55555555555555547}, expected_number{expected_class::finite, -5.2195060344100801}, expected_number{expected_class::finite, 0.63549841292334364}, expected_number{expected_class::finite, -0.80867566791151946}, expected_number{expected_class::finite, 0.99999999999999989}, expected_number{expected_class::finite, -0.026499999999999999}}
};
inline const std::array<std::array<expected_number, 8>, 13> performance_summary{{
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 3.4482867055563355}, expected_number{expected_class::finite, 0.064371442374687449}, expected_number{expected_class::finite, 8.2973336894420697}, expected_number{expected_class::finite, -0.6477815615951098}, expected_number{expected_class::finite, -0.43291121366957674}, expected_number{expected_class::finite, 1.1452491211762379}, expected_number{expected_class::finite, 17.613825263277036}, expected_number{expected_class::finite, 1.5781603348458146}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 0.011396852213023292}, expected_number{expected_class::finite, 0.001168068760191876}, expected_number{expected_class::finite, 0.044669921583277893}, expected_number{expected_class::finite, -0.033978998445299217}, expected_number{expected_class::finite, -0.017909710593064426}, expected_number{expected_class::finite, 0.01890668602797041}, expected_number{expected_class::finite, 0.088473158766688223}, expected_number{expected_class::finite, 0.036816396621034836}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 0.31064768267561194}, expected_number{expected_class::finite, 0.32828226500586777}, expected_number{expected_class::finite, 0.041528059770139902}, expected_number{expected_class::finite, 0.24332471704399589}, expected_number{expected_class::finite, 0.28518411029247559}, expected_number{expected_class::finite, 0.3341792761548672}, expected_number{expected_class::finite, 0.35776335246409713}, expected_number{expected_class::finite, 0.04899516586239161}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 1.8235512482318126}, expected_number{expected_class::finite, 0.20679299597968387}, expected_number{expected_class::finite, 5.4088124257311669}, expected_number{expected_class::finite, -3.1529040924996128}, expected_number{expected_class::finite, -1.4283456684500186}, expected_number{expected_class::finite, 2.2053343232103058}, expected_number{expected_class::finite, 11.348500865067583}, expected_number{expected_class::finite, 3.6336799916603244}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 323.68076246590039}, expected_number{expected_class::finite, 2.8890002801079526}, expected_number{expected_class::finite, 827.95839845719217}, expected_number{expected_class::finite, -14.181891881275034}, expected_number{expected_class::finite, -6.6524263242340673}, expected_number{expected_class::finite, 30.775070826214261}, expected_number{expected_class::finite, 1665.5538228861146}, expected_number{expected_class::finite, 37.427497150448332}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 0.28726412616636854}, expected_number{expected_class::finite, 0.20568104175219759}, expected_number{expected_class::finite, 0.27954795808243393}, expected_number{expected_class::finite, 0.029912381353791491}, expected_number{expected_class::finite, 0.078544731971491352}, expected_number{expected_class::finite, 0.36711440461964939}, expected_number{expected_class::finite, 0.76454926212507479}, expected_number{expected_class::finite, 0.28856967264815803}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, -0.040337521955436065}, expected_number{expected_class::finite, -0.039842059999999999}, expected_number{expected_class::finite, 0.017599217119429852}, expected_number{expected_class::finite, -0.066847135375000116}, expected_number{expected_class::finite, -0.046495683607616266}, expected_number{expected_class::finite, -0.030000000000000061}, expected_number{expected_class::finite, -0.01699999999999996}, expected_number{expected_class::finite, 0.016495683607616204}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 1.9512317747611867}, expected_number{expected_class::finite, 1.0384615384615383}, expected_number{expected_class::finite, 2.3363484344179315}, expected_number{expected_class::finite, 0.62361111111111101}, expected_number{expected_class::finite, 0.80514705882352922}, expected_number{expected_class::finite, 1.4278846153846154}, expected_number{expected_class::finite, 5.987499999999998}, expected_number{expected_class::finite, 0.6227375565610862}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 6.9143870296980348}, expected_number{expected_class::finite, 0.37169597083751393}, expected_number{expected_class::finite, 17.804954981697101}, expected_number{expected_class::finite, -4.2823633953896909}, expected_number{expected_class::finite, -1.9740641622727915}, expected_number{expected_class::finite, 3.595479766635596}, expected_number{expected_class::finite, 37.733776682342651}, expected_number{expected_class::finite, 5.5695439289083879}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, -0.17371582207185138}, expected_number{expected_class::finite, -0.13780376795472565}, expected_number{expected_class::finite, 0.43157575432736456}, expected_number{expected_class::finite, -0.78024279329232649}, expected_number{expected_class::finite, -0.39505240600598512}, expected_number{expected_class::finite, -0.019385661921056924}, expected_number{expected_class::finite, 0.44244807420205934}, expected_number{expected_class::finite, 0.37566674408492817}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, -1.1202939799401284}, expected_number{expected_class::finite, -1.1884914636660491}, expected_number{expected_class::finite, 0.21686434206655317}, expected_number{expected_class::finite, -1.3872570655644498}, expected_number{expected_class::finite, -1.2551626226525474}, expected_number{expected_class::finite, -0.95678775144948669}, expected_number{expected_class::finite, -0.80179194682834454}, expected_number{expected_class::finite, 0.29837487120306072}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, 1.2614636935391652}, expected_number{expected_class::finite, 0.99999999999999989}, expected_number{expected_class::finite, 0.90330296366772256}, expected_number{expected_class::finite, 0.75680817610062889}, expected_number{expected_class::finite, 0.88333333333333319}, expected_number{expected_class::finite, 0.99999999999999989}, expected_number{expected_class::finite, 2.7136363636363621}, expected_number{expected_class::finite, 0.1166666666666667}}},
    std::array<expected_number, 8>{{expected_number{expected_class::finite, -0.024281249999999997}, expected_number{expected_class::finite, -0.026499999999999999}, expected_number{expected_class::finite, 0.006712206115540552}, expected_number{expected_class::finite, -0.029999999999999999}, expected_number{expected_class::finite, -0.027375}, expected_number{expected_class::finite, -0.024875000000000001}, expected_number{expected_class::finite, -0.012362500000000005}, expected_number{expected_class::finite, 0.0024999999999999988}}}
}};

inline const std::vector<double> simulation_returns {0.01, std::numeric_limits<double>::quiet_NaN(), 0, -0.02};
inline const std::vector<std::vector<std::size_t>> simulation_draws {
    {1, 1, 1},
    {0, 0, 0},
    {2, 2, 2},
    {0, 1, 2},
    {2, 0, 2}
};
inline constexpr std::array<std::string_view, 7> simulation_names{{
    "mean",
    "sample_standard_deviation",
    "median",
    "sharpe_ratio",
    "sortino_ratio",
    "minimum",
    "maximum"
}};
inline const std::array<std::vector<expected_number>, 7> simulation_samples{{
    std::vector<expected_number>{expected_number{expected_class::finite, 0.0}, expected_number{expected_class::finite, 0.01}, expected_number{expected_class::finite, -0.02}, expected_number{expected_class::finite, -0.0033333333333333335}, expected_number{expected_class::finite, -0.01}},
    std::vector<expected_number>{expected_number{expected_class::finite, 0.0}, expected_number{expected_class::finite, 0.0}, expected_number{expected_class::finite, 0.0}, expected_number{expected_class::finite, 0.015275252316519468}, expected_number{expected_class::finite, 0.017320508075688773}},
    std::vector<expected_number>{expected_number{expected_class::finite, 0.0}, expected_number{expected_class::finite, 0.01}, expected_number{expected_class::finite, -0.02}, expected_number{expected_class::finite, 0.0}, expected_number{expected_class::finite, -0.02}},
    std::vector<expected_number>{expected_number{expected_class::nan, 0.0}, expected_number{expected_class::positive_infinity, 0.0}, expected_number{expected_class::positive_infinity, 0.0}, expected_number{expected_class::finite, -3.4641016151377544}, expected_number{expected_class::finite, -9.1651513899116797}},
    std::vector<expected_number>{expected_number{expected_class::nan, 0.0}, expected_number{expected_class::positive_infinity, 0.0}, expected_number{expected_class::positive_infinity, 0.0}, expected_number{expected_class::finite, -4.5825756949558398}, expected_number{expected_class::finite, -13.74772708486752}},
    std::vector<expected_number>{expected_number{expected_class::finite, 0.0}, expected_number{expected_class::finite, 0.01}, expected_number{expected_class::finite, -0.02}, expected_number{expected_class::finite, -0.02}, expected_number{expected_class::finite, -0.02}},
    std::vector<expected_number>{expected_number{expected_class::finite, 0.0}, expected_number{expected_class::finite, 0.01}, expected_number{expected_class::finite, -0.02}, expected_number{expected_class::finite, 0.01}, expected_number{expected_class::finite, 0.01}}
}};

}  // namespace bootstrap_fixed_indices_fixture
